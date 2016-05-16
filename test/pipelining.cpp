#include "generated.hpp"
#include "impl_test_interface.hpp"
#include "checkpoint.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/test/unit_test.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>

BOOST_AUTO_TEST_CASE(async_client_pipelining)
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
            auto server =
                std::make_shared<async_test_interface_server<decltype(server_impl), boost::asio::ip::tcp::socket,
                                                             boost::asio::ip::tcp::socket>>(
                    server_impl, accepted_socket, accepted_socket);
            server->serve_one_request([server, &served_0, &served_1, &got_1](boost::system::error_code ec)
                                      {
                                          BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                          served_0.enter();
                                          served_1.enable();
                                          got_1.enable();
                                          server->serve_one_request([server, &served_1](boost::system::error_code ec)
                                                                    {
                                                                        BOOST_REQUIRE_EQUAL(boost::system::error_code(),
                                                                                            ec);
                                                                        served_1.enter();
                                                                    });
                                      });
        });
    boost::asio::ip::tcp::socket socket(io);
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(socket, socket);
    warpcoil::checkpoint got_0;
    socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&client, &got_0, &got_1](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            client.utf8("X", [&got_0](boost::system::error_code ec, std::string result)
                        {
                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                            BOOST_CHECK_EQUAL("X123", result);
                            got_0.enter();
                        });
            client.utf8("Y", [&got_1](boost::system::error_code ec, std::string result)
                        {
                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                            BOOST_CHECK_EQUAL("Y123", result);
                            got_1.enter();
                        });
        });
    served_0.enable();
    got_0.enable();
    io.run();
}
