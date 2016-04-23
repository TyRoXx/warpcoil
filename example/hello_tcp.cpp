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
    acceptor.async_accept(accepted_socket, [&accepted_socket, &server_impl](boost::system::error_code ec)
                          {
                              Si::throw_if_error(ec);
                              auto server =
                                  std::make_shared<async_hello_as_a_service_server<ip::tcp::socket, ip::tcp::socket>>(
                                      server_impl, accepted_socket, accepted_socket);
                              server->serve_one_request([server](boost::system::error_code ec)
                                                        {
                                                            Si::throw_if_error(ec);
                                                        });
                          });

    // client:
    ip::tcp::socket connecting_socket(io);
    async_hello_as_a_service_client<ip::tcp::socket, ip::tcp::socket> client(connecting_socket, connecting_socket);
    connecting_socket.async_connect(ip::tcp::endpoint(ip::address_v4::loopback(), acceptor.local_endpoint().port()),
                                    [&io, &client](boost::system::error_code ec)
                                    {
                                        Si::throw_if_error(ec);
                                        std::string name;
                                        std::cin >> name;
                                        client.hello(std::move(name),
                                                     [](boost::system::error_code ec, std::string result)
                                                     {
                                                         Si::throw_if_error(ec);
                                                         std::cout << result << '\n';
                                                     });
                                    });

    io.run();
}
