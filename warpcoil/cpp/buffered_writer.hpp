#pragma once

#include <warpcoil/cpp/helpers.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/write.hpp>
#include <silicium/sink/iterator_sink.hpp>

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

            bool is_running() const
            {
                return begin_write != nullptr;
            }

            template <class CompletionToken>
            auto async_run(CompletionToken &&token)
            {
                assert(!begin_write);
                using handler_type =
                    typename boost::asio::handler_type<decltype(token), void(boost::system::error_code)>::type;
                handler_type handler(std::forward<CompletionToken>(token));
                boost::asio::async_result<handler_type> result(handler);
                begin_write = [this, handler]()
                {
                    boost::asio::async_write(destination, boost::asio::buffer(being_written),
                                             write_operation<decltype(handler)>(*this, handler));
                };
                return result.get();
            }

            auto buffer_sink()
            {
                return Si::make_container_sink(buffer);
            }

            template <class ErrorHandler>
            void send_buffer(ErrorHandler on_result)
            {
                assert(!buffer.empty());
                this->on_result =
                    [ next_on_result = this->on_result, on_result ](boost::system::error_code const ec) mutable
                {
                    if (next_on_result)
                    {
                        next_on_result(ec);
                    }
                    on_result(ec);
                };
                if (!begin_write)
                {
                    return;
                }
                if (being_written.empty())
                {
                    being_written = std::move(buffer);
                    assert(buffer.empty());
                    begin_write();
                }
            }

        private:
            AsyncWriteStream &destination;
            std::function<void()> begin_write;
            std::function<void(boost::system::error_code)> on_result;
            std::vector<std::uint8_t> being_written;
            std::vector<std::uint8_t> buffer;

            template <class ErrorHandler>
            struct write_operation
            {
                buffered_writer &writer;
                ErrorHandler handler;

                explicit write_operation(buffered_writer &writer, ErrorHandler handler)
                    : writer(writer)
                    , handler(std::move(handler))
                {
                }

                void operator()(boost::system::error_code ec, std::size_t)
                {
                    auto local_on_result = std::move(writer.on_result);
                    assert(!writer.on_result);
                    local_on_result(ec);
                    if (!!ec)
                    {
                        handler(ec);
                        return;
                    }
                    writer.being_written.clear();
                    if (!writer.buffer.empty())
                    {
                        writer.send_buffer([](boost::system::error_code const)
                                           {
                                           });
                    }
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f, write_operation *operation)
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(f, &operation->handler);
                }
            };
        };
    }
}
