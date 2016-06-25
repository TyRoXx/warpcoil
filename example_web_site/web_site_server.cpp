#include "web_site_interfaces.hpp"
#include <warpcoil/beast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <silicium/error_or.hpp>
#include <iostream>
#include <warpcoil/beast_push_warnings.hpp>
#include <beast/websocket/stream.hpp>
#include <warpcoil/pop_warnings.hpp>

namespace
{
    struct my_service : async_web_site_service
    {
        void hello(std::string argument, std::function<void(boost::system::error_code, std::string)> on_result) override
        {
            on_result({}, "Hello, " + argument + "!");
        }
    };

    void begin_accept(boost::asio::ip::tcp::acceptor &acceptor, my_service &server_impl)
    {
        auto accepted_socket = std::make_shared<boost::asio::ip::tcp::socket>(acceptor.get_io_service());
        acceptor.async_accept(
            *accepted_socket, [&acceptor, accepted_socket, &server_impl](boost::system::error_code ec)
            {
                Si::throw_if_error(ec);
                begin_accept(acceptor, server_impl);

                typedef beast::websocket::stream<boost::asio::ip::tcp::socket &> websocket;
                auto session = std::make_shared<warpcoil::beast::async_stream_adaptor<websocket>>(*accepted_socket);
                session->next_layer().async_accept(
                    [session, &server_impl, accepted_socket](boost::system::error_code ec)
                    {
                        Si::throw_if_error(ec);
                        auto splitter = std::make_shared<warpcoil::cpp::message_splitter<decltype(*session)>>(*session);
                        auto writer = std::make_shared<warpcoil::cpp::buffered_writer<decltype(*session)>>(*session);
                        auto server =
                            std::make_shared<async_web_site_service_server<decltype(server_impl), decltype(*session),
                                                                           decltype(*session)>>(server_impl, *splitter,
                                                                                                *writer);
                        server->serve_one_request(
                            [server, writer, session, splitter, accepted_socket](boost::system::error_code ec)
                            {
                                Si::throw_if_error(ec);
                            });
                    });
            });
    }
}

int main()
{
    using namespace boost::asio;
    io_service io;

    my_service server_impl;

    boost::uint16_t const port = 8080;

    ip::tcp::acceptor acceptor_v6(io, ip::tcp::endpoint(ip::tcp::v6(), port), true);
    acceptor_v6.listen();
    begin_accept(acceptor_v6, server_impl);

    ip::tcp::acceptor acceptor_v4(io, ip::tcp::endpoint(ip::tcp::v4(), port), true);
    acceptor_v4.listen();
    begin_accept(acceptor_v4, server_impl);

    while (!io.stopped())
    {
        try
        {
            io.run();
        }
        catch (std::exception const &ex)
        {
            std::cerr << "Exception: " << ex.what() << '\n';
        }
    }
}
