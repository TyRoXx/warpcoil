#include "generated.hpp"
#include "impl_test_interface.hpp"
#include "checkpoint.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(bidirectional)
{
    return; // TODO: sending both requests and responses on the same socket

    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::ip::tcp::socket accepted_socket(io);
    warpcoil::impl_test_interface server_impl_a;
    acceptor.async_accept(
        accepted_socket, [&accepted_socket, &server_impl_a](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            auto splitter_a =
                std::make_shared<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>>(accepted_socket);
            async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client_a(
                accepted_socket, *splitter_a);
            client_a.utf8("Y", [splitter_a](boost::system::error_code ec, std::string result)
                          {
                              BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                              BOOST_CHECK_EQUAL("Y123", result);
                          });
            BOOST_CHECK_EQUAL(1u, pending_requests(client_a));
            auto server_a =
                std::make_shared<async_test_interface_server<decltype(server_impl_a), boost::asio::ip::tcp::socket,
                                                             boost::asio::ip::tcp::socket>>(server_impl_a, *splitter_a,
                                                                                            accepted_socket);
            server_a->serve_one_request([server_a, splitter_a](boost::system::error_code ec)
                                        {
                                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                        });
        });
    boost::asio::ip::tcp::socket connecting_socket(io);
    warpcoil::cpp::message_splitter<decltype(connecting_socket)> splitter_b(connecting_socket);
    warpcoil::impl_test_interface server_impl_b;
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(connecting_socket,
                                                                                                   splitter_b);
    connecting_socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&client, &server_impl_b, &splitter_b, &connecting_socket](boost::system::error_code ec)
        {
            BOOST_CHECK_EQUAL(0u, pending_requests(client));
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            auto server_b =
                std::make_shared<async_test_interface_server<decltype(server_impl_b), boost::asio::ip::tcp::socket,
                                                             boost::asio::ip::tcp::socket>>(server_impl_b, splitter_b,
                                                                                            connecting_socket);
            client.utf8("X", [](boost::system::error_code ec, std::string result)
                        {
                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                            BOOST_CHECK_EQUAL("X123", result);
                        });
            BOOST_CHECK_EQUAL(1u, pending_requests(client));
            server_b->serve_one_request([server_b](boost::system::error_code ec)
                                        {
                                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                        });
        });
    io.run();
    BOOST_CHECK_EQUAL(0u, pending_requests(client));
}
