#pragma once

#include <boost/asio/write.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/variant.hpp>
#include <warpcoil/cpp/wrap_handler.hpp>
#include <silicium/memory_range.hpp>
#include <silicium/make_unique.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class AsyncWriteStream>
        struct buffered_writer
        {
            explicit buffered_writer(AsyncWriteStream &destination)
                : destination(destination)
            {
            }

            auto buffer_sink()
            {
                return Si::make_container_sink(buffer);
            }

            template <class ErrorHandler>
            void send_buffer(ErrorHandler on_result)
            {
                assert(!buffer.empty());
                return Si::visit<void>(
                    state,
                    [this, &on_result](not_sending &)
                    {
                        std::tuple<std::shared_ptr<std::unique_ptr<result_handler_base>>, Si::memory_range> prepared =
                            prepare_send_state();
                        auto const sent =
                            boost::asio::buffer(std::get<1>(prepared).begin(), std::get<1>(prepared).size());
                        boost::asio::async_write(
                            destination, sent,
                            wrap_handler([ this, buffer_handler_queue = std::get<0>(prepared) ](
                                             boost::system::error_code const ec, std::size_t, ErrorHandler &on_result)
                                         {
                                             finish_send(ec, buffer_handler_queue, on_result);
                                         },
                                         on_result));
                    },
                    [&on_result](sending &current_state)
                    {
                        std::shared_ptr<std::unique_ptr<result_handler_base>> buffer_handler_queue =
                            current_state.buffer_handler_queue.lock();
                        assert(buffer_handler_queue);
                        std::unique_ptr<result_handler_base> next_handler = std::move(*buffer_handler_queue);
                        if (next_handler)
                        {
                            *buffer_handler_queue =
                                Si::make_unique<result_handler<ErrorHandler>>(on_result, std::move(next_handler));
                        }
                        else
                        {
                            *buffer_handler_queue = Si::make_unique<result_handler<ErrorHandler>>(on_result);
                        }
                    });
            }

        private:
            struct result_handler_base
            {
                virtual ~result_handler_base()
                {
                }
                virtual void handle_result(boost::system::error_code ec) = 0;
                virtual void async_send(buffered_writer &writer,
                                        std::shared_ptr<std::unique_ptr<result_handler_base>> buffer_handler_queue,
                                        Si::memory_range data,
                                        std::unique_ptr<result_handler_base> ownership_of_this) = 0;
            };

            template <class Handler>
            struct result_handler : result_handler_base
            {
                explicit result_handler(Handler handler, std::unique_ptr<result_handler_base> next)
                    : handler(std::move(handler))
                    , next(std::move(next))
                {
                    assert(this->next);
                }

                explicit result_handler(Handler handler)
                    : handler(std::move(handler))
                {
                }

                void handle_result(boost::system::error_code ec) override
                {
                    if (next)
                    {
                        next->handle_result(ec);
                    }
                    handler(ec);
                }

                void async_send(buffered_writer &writer,
                                std::shared_ptr<std::unique_ptr<result_handler_base>> buffer_handler_queue,
                                Si::memory_range data, std::unique_ptr<result_handler_base> ownership_of_this) override
                {
                    assert(!data.empty());
                    assert(ownership_of_this);
                    boost::asio::async_write(
                        writer.destination, boost::asio::buffer(data.begin(), data.size()),
                        wrap_handler(
                            [
                              this,
                              &writer,
                              buffer_handler_queue,
                              ownership_of_this = std::shared_ptr<result_handler_base>(std::move(ownership_of_this))
                            ](boost::system::error_code const ec, std::size_t, Handler &)
                            {
                                writer.finish_send(ec, buffer_handler_queue, [this](boost::system::error_code const ec)
                                                   {
                                                       this->handle_result(ec);
                                                   });
                            },
                            handler));
                }

            private:
                Handler handler;
                std::unique_ptr<result_handler_base> next;
            };

            struct not_sending
            {
            };

            struct sending
            {
                std::vector<std::uint8_t> being_written;
                std::weak_ptr<std::unique_ptr<result_handler_base>> buffer_handler_queue;

                sending(std::vector<std::uint8_t> being_written,
                        std::weak_ptr<std::unique_ptr<result_handler_base>> buffer_handler_queue)
                    : being_written(std::move(being_written))
                    , buffer_handler_queue(std::move(buffer_handler_queue))
                {
                }

                sending(sending &&) BOOST_NOEXCEPT = default;
                sending &operator=(sending &&) BOOST_NOEXCEPT = default;
                sending(sending const &) BOOST_NOEXCEPT = delete;
                sending &operator=(sending const &) BOOST_NOEXCEPT = delete;
            };

            AsyncWriteStream &destination;
            std::vector<std::uint8_t> buffer;
            Si::variant<not_sending, sending> state;

            std::tuple<std::shared_ptr<std::unique_ptr<result_handler_base>>, Si::memory_range> prepare_send_state()
            {
                assert(!buffer.empty());
                std::shared_ptr<std::unique_ptr<result_handler_base>> buffer_handler_queue =
                    std::make_shared<std::unique_ptr<result_handler_base>>();
                sending new_state(std::move(buffer), buffer_handler_queue);
                buffer = std::vector<std::uint8_t>();
                assert(buffer.empty());
                Si::memory_range const being_written = Si::make_memory_range(new_state.being_written);
                assert(!new_state.being_written.empty());
                new_state.buffer_handler_queue = buffer_handler_queue;
                state = std::move(new_state);
                return std::make_tuple(buffer_handler_queue, being_written);
            }

            void continue_send(sending &current_state, std::unique_ptr<result_handler_base> handlers)
            {
                assert(handlers);
                assert(!buffer.empty());
                assert(!current_state.being_written.empty());
                std::tuple<std::shared_ptr<std::unique_ptr<result_handler_base>>, Si::memory_range> prepared =
                    prepare_send_state();
                assert(std::get<0>(prepared));
                assert(!*std::get<0>(prepared));
                assert(!std::get<1>(prepared).empty());
                current_state.buffer_handler_queue = std::get<0>(prepared);
                result_handler_base &sender = *handlers;
                sender.async_send(*this, std::get<0>(prepared), std::get<1>(prepared), std::move(handlers));
            }

            template <class ErrorCodeHandler>
            void finish_send(boost::system::error_code const ec,
                             std::shared_ptr<std::unique_ptr<result_handler_base>> buffer_handler_queue,
                             ErrorCodeHandler &&on_result)
            {
                assert(buffer_handler_queue);
                if (buffer.empty())
                {
                    state = not_sending();
                    on_result(ec);
                }
                else
                {
                    if (*buffer_handler_queue)
                    {
                        // there were calls to send_buffer in the meantime
                        on_result(ec);
                        sending *const current_state = Si::try_get_ptr<sending>(state);
                        assert(current_state);
                        continue_send(*current_state, std::move(*buffer_handler_queue));
                    }
                    else
                    {
                        // the buffer was appended to, but send_buffer has not been called
                        // for the new data yet
                        state = not_sending();
                        on_result(ec);
                    }
                }
            }
        };
    }
}
