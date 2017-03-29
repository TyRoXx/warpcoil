#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <silicium/error_or.hpp>
#include <silicium/variant.hpp>

namespace warpcoil
{
    struct pipe
    {
        explicit pipe(boost::asio::io_service &io, size_t max_size)
            : m_io(io)
            , m_max_size(max_size)
        {
        }

        template <class CompletionToken>
        auto read(CompletionToken &&token)
        {
            using handler_type =
                typename boost::asio::handler_type<decltype(token),
                                                   void(Si::error_or<std::vector<std::uint8_t>>)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            if (m_buffer.empty())
            {
                m_on_read = std::move(handler);
            }
            else
            {
                m_io.post([ buffer = std::move(m_buffer), handler = std::move(handler) ]() mutable
                          {
                              handler(std::move(buffer));
                          });
                assert(m_buffer.empty());
                if (m_on_write)
                {
                    m_io.post([on_write = std::move(m_on_write)]
                              {
                                  on_write(std::make_tuple());
                              });
                    assert(!m_on_write);
                }
            }
            return result.get();
        }

        template <class CompletionToken>
        auto write(std::vector<std::uint8_t> element, CompletionToken &&token)
        {
            using handler_type =
                typename boost::asio::handler_type<decltype(token), void(Si::error_or<std::tuple<>>)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            assert(!m_on_write);
            if (m_buffer.empty())
            {
                m_buffer = std::move(element);
            }
            else
            {
                m_buffer.insert(m_buffer.end(), element.begin(), element.end());
            }
            if (m_on_read)
            {
                m_io.post([ on_read = std::move(m_on_read), buffer = std::move(m_buffer) ]() mutable
                          {
                              on_read(std::move(buffer));
                          });
                assert(!m_on_read);
                assert(m_buffer.empty());
            }
            if (m_buffer.size() >= m_max_size)
            {
                m_on_write = std::move(handler);
            }
            else
            {
                m_io.post([handler = std::move(handler)]() mutable
                          {
                              handler(std::make_tuple());
                          });
            }
            return result.get();
        }

    private:
        boost::asio::io_service &m_io;
        std::vector<boost::uint8_t> m_buffer;
        size_t m_max_size;
        std::function<void(Si::error_or<std::vector<std::uint8_t>>)> m_on_read;
        std::function<void(Si::error_or<std::tuple<>>)> m_on_write;
    };
}

BOOST_AUTO_TEST_CASE(asio_spawn_concurrency)
{
    size_t const copying = 5;
    for (size_t buffer_size = 1; buffer_size <= (copying + 1); ++buffer_size)
    {
        boost::asio::io_service io;
        warpcoil::pipe transfer(io, buffer_size);
        boost::uint8_t const element = 23;

        // produce
        boost::asio::spawn(io, [&](boost::asio::yield_context yield)
                           {
                               size_t i = 0;
                               for (; i < copying; ++i)
                               {
                                   transfer.write(std::vector<boost::uint8_t>(1, element), yield);
                               }
                               BOOST_TEST(copying == i);
                           });

        bool ok = false;
        // consume
        boost::asio::spawn(io, [&](boost::asio::yield_context yield)
                           {
                               size_t i = 0;
                               for (; i < copying;)
                               {
                                   std::vector<boost::uint8_t> received = transfer.read(yield).get();
                                   BOOST_TEST(boost::algorithm::all_of_equal(received, element));
                                   i += received.size();
                               }
                               BOOST_TEST(copying == i);
                               ok = true;
                           });

        BOOST_TEST(!ok);
        io.run();
        BOOST_TEST(ok);
    }
}

BOOST_AUTO_TEST_CASE(pipe_write_buffering)
{
    boost::asio::io_service io;
    warpcoil::pipe transfer(io, 3);
    boost::uint8_t const element = 23;

    bool ok = false;
    boost::asio::spawn(io, [&](boost::asio::yield_context yield)
                       {
                           transfer.write(std::vector<boost::uint8_t>(1, element), yield);
                           transfer.write(std::vector<boost::uint8_t>(1, element), yield);
                           std::vector<boost::uint8_t> received = transfer.read(yield).get();
                           std::vector<boost::uint8_t> const expected = {element, element};
                           BOOST_TEST(expected == received);
                           ok = true;
                       });

    BOOST_TEST(!ok);
    io.run();
    BOOST_TEST(ok);
}

namespace warpcoil
{
    constexpr struct use_future_type
    {
    } use_future;

    namespace detail
    {
        template <class Result>
        struct future_handler
        {
            std::shared_ptr<Si::variant<Si::unit, std::function<void(Result)>, Result>> state;

            explicit future_handler(use_future_type)
                : state(std::make_shared<Si::variant<Si::unit, std::function<void(Result)>, Result>>())
            {
            }

            void operator()(Result result)
            {
                Si::visit<void>(*state,
                                [this, &result](Si::unit)
                                {
                                    *state = std::move(result);
                                },
                                [&result](std::function<void(Result)> &handler)
                                {
                                    handler(std::move(result));
                                },
                                [](Result &)
                                {
                                    SILICIUM_UNREACHABLE();
                                });
            }
        };
    }

    template <class Result>
    struct future
    {
        explicit future(std::function<void(std::function<void(Result)>)> wait)
            : _wait(std::move(wait))
        {
        }

        template <class CompletionToken>
        auto async_wait(CompletionToken &&token)
        {
            using handler_type = typename boost::asio::handler_type<decltype(token), void(Result)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            _wait(std::move(handler));
            return result.get();
        }

    private:
        std::function<void(std::function<void(Result)>)> _wait;
    };
}

namespace boost
{
    namespace asio
    {
        template <class Result>
        struct async_result<warpcoil::detail::future_handler<Result>>
        {
            explicit async_result(warpcoil::detail::future_handler<Result> const &handler)
                : _handler(handler)
            {
                assert(_handler.state);
            }

            warpcoil::future<Result> get()
            {
                assert(_handler.state);
                return warpcoil::future<Result>([state = std::move(_handler.state)](
                    std::function<void(Result)> handle_result)
                                                {
                                                    Si::visit<void>(*state,
                                                                    [&state, &handle_result](Si::unit)
                                                                    {
                                                                        *state = std::move(handle_result);
                                                                    },
                                                                    [](std::function<void(Result)> &)
                                                                    {
                                                                        SILICIUM_UNREACHABLE();
                                                                    },
                                                                    [&handle_result](Result &result)
                                                                    {
                                                                        handle_result(std::move(result));
                                                                    });
                                                });
            }

        private:
            warpcoil::detail::future_handler<Result> _handler;
        };

        template <class Result>
        struct handler_type<warpcoil::use_future_type, void(Result)>
        {
            using type = warpcoil::detail::future_handler<Result>;
        };
    }
}

BOOST_AUTO_TEST_CASE(asio_spawn_future)
{
    size_t const copying = 5;
    for (size_t buffer_size = 1; buffer_size <= (copying + 1); ++buffer_size)
    {
        boost::asio::io_service io;
        warpcoil::pipe transfer(io, buffer_size);
        boost::uint8_t const element = 23;

        // produce
        boost::asio::spawn(io, [&](boost::asio::yield_context yield)
                           {
                               size_t i = 0;
                               for (; i < copying; ++i)
                               {
                                   warpcoil::future<Si::error_or<std::tuple<>>> writing =
                                       transfer.write(std::vector<boost::uint8_t>(1, element), warpcoil::use_future);
                                   writing.async_wait(yield);
                               }
                               BOOST_TEST(copying == i);
                           });

        bool ok = false;
        // consume
        boost::asio::spawn(io, [&](boost::asio::yield_context yield)
                           {
                               size_t i = 0;
                               for (; i < copying;)
                               {
                                   warpcoil::future<Si::error_or<std::vector<boost::uint8_t>>> reading =
                                       transfer.read(warpcoil::use_future);
                                   std::vector<boost::uint8_t> received = reading.async_wait(yield).get();
                                   BOOST_TEST(boost::algorithm::all_of_equal(received, element));
                                   i += received.size();
                               }
                               BOOST_TEST(copying == i);
                               ok = true;
                           });

        BOOST_TEST(!ok);
        io.run();
        BOOST_TEST(ok);
    }
}
