#include "generated.hpp"
#include "impl_test_interface.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>
#include <silicium/error_or.hpp>
#include <beast/websocket.hpp>
#include <warpcoil/beast.hpp>
#include "checkpoint.hpp"

BOOST_AUTO_TEST_CASE(websocket)
{
    using namespace boost::asio;
    io_service io;

    warpcoil::checkpoint tcp_accepted;
    warpcoil::checkpoint websocket_accepted;
    warpcoil::checkpoint served;
    warpcoil::checkpoint tcp_connected;
    warpcoil::checkpoint websocket_connected;
    warpcoil::checkpoint received_response;

    ip::tcp::acceptor acceptor(io, ip::tcp::endpoint(ip::tcp::v6(), 0), true);
    acceptor.listen();
    ip::tcp::socket accepted_socket(io);
    warpcoil::impl_test_interface server_impl;
    acceptor.async_accept(
        accepted_socket, [&](boost::system::error_code ec)
        {
            tcp_accepted.enter();
            websocket_connected.enable();
            Si::throw_if_error(ec);
            typedef beast::websocket::stream<ip::tcp::socket &> websocket;
            auto session = std::make_shared<warpcoil::beast::async_stream_adaptor<websocket>>(accepted_socket);
            session->next_layer().async_accept(
                [session, &server_impl, &websocket_accepted, &served, &received_response](boost::system::error_code ec)
                {
                    websocket_accepted.enter();
                    received_response.enable();
                    Si::throw_if_error(ec);
                    auto server = std::make_shared<async_test_interface_server<decltype(*session), decltype(*session)>>(
                        server_impl, *session, *session);
                    server->serve_one_request([server, session, &served](boost::system::error_code ec)
                                              {
                                                  served.enter();
                                                  Si::throw_if_error(ec);
                                              });
                });
        });

    ip::tcp::socket connecting_socket(io);
    connecting_socket.async_connect(
        ip::tcp::endpoint(ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&](boost::system::error_code ec)
        {
            tcp_connected.enter();
            websocket_accepted.enable();
            Si::throw_if_error(ec);
            typedef beast::websocket::stream<ip::tcp::socket &> websocket;
            auto session = std::make_shared<warpcoil::beast::async_stream_adaptor<websocket>>(connecting_socket);
            session->next_layer().async_handshake(
                "localhost", "/",
                [session, &websocket_connected, &received_response, &served](boost::system::error_code ec)
                {
                    websocket_connected.enter();
                    served.enable();
                    Si::throw_if_error(ec);
                    auto client = std::make_shared<async_test_interface_client<decltype(*session), decltype(*session)>>(
                        *session, *session);
                    std::string name = "Alice";
                    client->utf8(std::move(name),
                                 [client, session, &received_response](boost::system::error_code ec, std::string result)
                                 {
                                     received_response.enter();
                                     Si::throw_if_error(ec);
                                     BOOST_CHECK_EQUAL("Alice123", result);
                                 });
                });
        });

    tcp_accepted.enable();
    tcp_connected.enable();
    io.run();
}
