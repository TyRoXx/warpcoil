#include "generated.hpp"
#include "impl_test_interface.hpp"
#include "checkpoint.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(async_client_pipelining_simple)
{
    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::ip::tcp::socket accepted_socket(io);
    warpcoil::checkpoint served_0;
    warpcoil::checkpoint served_1;
    warpcoil::checkpoint got_1;
    warpcoil::impl_test_interface server_impl;
    acceptor.async_accept(
        accepted_socket, [&accepted_socket, &server_impl, &served_0, &served_1, &got_1](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            auto splitter =
                std::make_shared<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>>(accepted_socket);
            auto writer =
                std::make_shared<warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket>>(accepted_socket);
            auto server =
                std::make_shared<async_test_interface_server<decltype(server_impl), boost::asio::ip::tcp::socket,
                                                             boost::asio::ip::tcp::socket>>(server_impl, *splitter,
                                                                                            *writer);
            server->serve_one_request(
                [server, writer, splitter, &served_0, &served_1, &got_1](boost::system::error_code ec)
                {
                    BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                    served_0.enter();
                    served_1.enable();
                    got_1.enable();
                    server->serve_one_request([server, writer, splitter, &served_1](boost::system::error_code ec)
                                              {
                                                  BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                                  served_1.enter();
                                              });
                });
        });
    boost::asio::ip::tcp::socket socket(io);
    warpcoil::cpp::message_splitter<decltype(socket)> splitter(socket);
    warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket> writer(socket);
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(writer, splitter);
    warpcoil::checkpoint got_0;
    socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&client, &writer, &got_0, &got_1](boost::system::error_code ec)
        {
            BOOST_CHECK_EQUAL(0u, pending_requests(client));
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            client.utf8("X", [&got_0](boost::system::error_code ec, std::string result)
                        {
                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                            BOOST_CHECK_EQUAL("X123", result);
                            got_0.enter();
                        });
            BOOST_CHECK_EQUAL(1u, pending_requests(client));
            client.utf8("Y", [&got_1](boost::system::error_code ec, std::string result)
                        {
                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                            BOOST_CHECK_EQUAL("Y123", result);
                            got_1.enter();
                        });
            BOOST_CHECK_EQUAL(2u, pending_requests(client));
        });
    served_0.enable();
    got_0.enable();
    io.run();
    BOOST_CHECK_EQUAL(0u, pending_requests(client));
}

namespace
{
    template <class ServerPtr>
    void serve_n(ServerPtr server,
                 std::shared_ptr<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>> server_splitter,
                 std::shared_ptr<warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket>> writer,
                 std::size_t const n)
    {
        if (n == 0)
        {
            return;
        }
        server->serve_one_request([server, server_splitter, writer, n](boost::system::error_code ec)
                                  {
                                      BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                      serve_n(server, server_splitter, writer, n - 1);
                                  });
    }

    template <class Client>
    void count_up(Client &client, std::uint16_t const first, std::uint16_t const last)
    {
        BOOST_CHECK_EQUAL(0u, pending_requests(client));
        client.variant(
            static_cast<std::uint32_t>(first),
            [&client, first, last](boost::system::error_code ec, Si::variant<std::uint16_t, std::string> result)
            {
                BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                BOOST_REQUIRE_EQUAL((Si::variant<std::uint16_t, std::string>{static_cast<std::uint16_t>(first + 1)}),
                                    result);
                if (first == last)
                {
                    return;
                }
                count_up(client, static_cast<std::uint16_t>(first + 1), last);
            });
        BOOST_CHECK_EQUAL(1u, pending_requests(client));
    }
}

BOOST_AUTO_TEST_CASE(async_client_pipelining_many_requests_in_sequence)
{
    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::ip::tcp::socket accepted_socket(io);
    warpcoil::impl_test_interface server_impl;
    std::uint16_t const request_count = 50;
    acceptor.async_accept(
        accepted_socket, [&accepted_socket, &server_impl, request_count](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            auto splitter =
                std::make_shared<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>>(accepted_socket);
            auto writer =
                std::make_shared<warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket>>(accepted_socket);
            auto server =
                std::make_shared<async_test_interface_server<decltype(server_impl), boost::asio::ip::tcp::socket,
                                                             boost::asio::ip::tcp::socket>>(server_impl, *splitter,
                                                                                            *writer);
            serve_n(server, splitter, writer, request_count);
        });
    boost::asio::ip::tcp::socket socket(io);
    warpcoil::cpp::message_splitter<decltype(socket)> splitter(socket);
    warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket> writer(socket);
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(writer, splitter);
    socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&client, request_count](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            count_up(client, 1, request_count);
        });
    io.run();
    BOOST_CHECK_EQUAL(0u, pending_requests(client));
}

BOOST_AUTO_TEST_CASE(async_client_pipelining_many_requests_in_parallel)
{
    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::ip::tcp::socket accepted_socket(io);
    warpcoil::impl_test_interface server_impl;
    std::uint16_t const request_count = 50;
    acceptor.async_accept(
        accepted_socket, [&accepted_socket, &server_impl, request_count](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            auto splitter =
                std::make_shared<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>>(accepted_socket);
            auto writer =
                std::make_shared<warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket>>(accepted_socket);
            auto server =
                std::make_shared<async_test_interface_server<decltype(server_impl), boost::asio::ip::tcp::socket,
                                                             boost::asio::ip::tcp::socket>>(server_impl, *splitter,
                                                                                            *writer);
            serve_n(server, splitter, writer, request_count);
        });
    boost::asio::ip::tcp::socket socket(io);
    warpcoil::cpp::message_splitter<decltype(socket)> splitter(socket);
    warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket> writer(socket);
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(writer, splitter);
    std::size_t got_responses = 0;
    socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&client, &writer, request_count, &got_responses](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            for (std::size_t i = 1; i <= request_count; ++i)
            {
                BOOST_CHECK_EQUAL(i - 1, pending_requests(client));
                client.variant(
                    static_cast<std::uint32_t>(i),
                    [i, &got_responses](boost::system::error_code ec, Si::variant<std::uint16_t, std::string> result)
                    {
                        BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                        BOOST_REQUIRE_EQUAL(
                            (Si::variant<std::uint16_t, std::string>{static_cast<std::uint16_t>(i + 1)}), result);
                        ++got_responses;
                    });
                BOOST_CHECK_EQUAL(i, pending_requests(client));
            }
        });
    io.run();
    BOOST_CHECK_EQUAL(request_count, got_responses);
    BOOST_CHECK_EQUAL(0u, pending_requests(client));
}
