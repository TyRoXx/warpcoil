#include "generated.hpp"
#include "impl_test_interface.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>

BOOST_AUTO_TEST_CASE(async_server_with_asio_spawn)
{
    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::ip::tcp::socket accepted_socket(io);
    bool ok1 = false;
    warpcoil::impl_test_interface server_impl;
    acceptor.async_accept(
        accepted_socket, [&io, &accepted_socket, &ok1, &server_impl](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            boost::asio::spawn(
                io, [&server_impl, &accepted_socket, &ok1](boost::asio::yield_context yield)
                {
                    auto server_splitter =
                        std::make_shared<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>>(
                            accepted_socket);
                    auto writer =
                        std::make_shared<warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket>>(accepted_socket);
                    async_test_interface_server<decltype(server_impl), boost::asio::ip::tcp::socket,
                                                boost::asio::ip::tcp::socket> server(server_impl, *server_splitter,
                                                                                     *writer);
                    server.serve_one_request(yield);
                    server.serve_one_request(yield);
                    BOOST_REQUIRE(!ok1);
                    ok1 = true;
                });
        });
    boost::asio::ip::tcp::socket socket(io);
    warpcoil::cpp::message_splitter<decltype(socket)> splitter(socket);
    warpcoil::cpp::buffered_writer<decltype(socket)> writer(socket);
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(writer, splitter);
    bool ok2 = false;
    socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&io, &writer, &client, &ok2](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            boost::asio::spawn(io, [&client, &ok2](boost::asio::yield_context yield)
                               {
                                   Si::error_or<std::vector<std::uint64_t>> result =
                                       client.vectors(std::vector<std::uint64_t>{12, 34, 56}, yield);
                                   std::vector<std::uint64_t> const expected{56, 34, 12};
                                   BOOST_CHECK(expected == result.get());

                                   BOOST_CHECK_EQUAL("hello123", client.utf8("hello", yield).get());

                                   BOOST_REQUIRE(!ok2);
                                   ok2 = true;
                               });
        });
    io.run();
    BOOST_CHECK(ok1);
    BOOST_CHECK(ok2);
}

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
