#include "hello_interfaces.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <silicium/error_or.hpp>
#include <iostream>

namespace server
{
    struct my_hello_service : async_hello_as_a_service
    {
        virtual void hello(std::string argument,
                           std::function<void(boost::system::error_code, std::string)> on_result) override
        {
            on_result({}, "Hello, " + argument + "!");
        }
    };
}

int main()
{
    using namespace boost::asio;
    io_service io;

    // server:
    ip::tcp::acceptor acceptor(io, ip::tcp::endpoint(ip::tcp::v6(), 0), true);
    acceptor.listen();
    ip::tcp::socket accepted_socket(io);
    server::my_hello_service server_impl;
    acceptor.async_accept(
        accepted_socket, [&accepted_socket, &server_impl](boost::system::error_code ec)
        {
            Si::throw_if_error(ec);
            auto splitter = std::make_shared<warpcoil::cpp::message_splitter<ip::tcp::socket>>(accepted_socket);
            auto writer = std::make_shared<warpcoil::cpp::buffered_writer<ip::tcp::socket>>(accepted_socket);
            auto server = std::make_shared<
                async_hello_as_a_service_server<decltype(server_impl), ip::tcp::socket, ip::tcp::socket>>(
                server_impl, *splitter, *writer);
            server->serve_one_request([server, splitter, writer](boost::system::error_code ec)
                                      {
                                          Si::throw_if_error(ec);
                                      });
        });

    // client:
    ip::tcp::socket connecting_socket(io);
    warpcoil::cpp::message_splitter<decltype(connecting_socket)> splitter(connecting_socket);
    warpcoil::cpp::buffered_writer<ip::tcp::socket> writer(connecting_socket);
    async_hello_as_a_service_client<ip::tcp::socket, ip::tcp::socket> client(writer, splitter);
    connecting_socket.async_connect(ip::tcp::endpoint(ip::address_v4::loopback(), acceptor.local_endpoint().port()),
                                    [&io, &client](boost::system::error_code ec)
                                    {
                                        Si::throw_if_error(ec);
                                        client.hello("Alice", [](boost::system::error_code ec, std::string result)
                                                     {
                                                         Si::throw_if_error(ec);
                                                         std::cout << result << '\n';
                                                     });
                                    });

    io.run();
}
