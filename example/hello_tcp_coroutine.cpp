#include "hello_interfaces.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <iostream>

namespace server
{
    struct my_hello_service : async_hello_as_a_service
    {
        void hello(std::string argument, std::function<void(boost::system::error_code, std::string)> on_result) override
        {
            on_result({}, "Hello, " + argument + "!");
        }
    };
}

int main()
{
    using namespace boost::asio;
    io_service io;

    // On Windows this example might not work.
    // boost::asio::spawn can make the creation of a socket crash with an Access Violation.
    // WSASocketW indirectly calls LoadLibraryExW which is not fully supported by Boost Coroutines.
    // http://boost.2283326.n4.nabble.com/boost-context-cause-strange-crash-on-certan-WIN32-API-td4669587.html

    // server:
    ip::tcp::acceptor acceptor(io, ip::tcp::endpoint(ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::spawn(
        io, [&io, &acceptor](boost::asio::yield_context yield)
        {
            ip::tcp::socket accepted_socket(io);
            server::my_hello_service server_impl;
            acceptor.async_accept(accepted_socket, yield);
            async_hello_as_a_service_server<decltype(server_impl), ip::tcp::socket, ip::tcp::socket> server(
                server_impl, accepted_socket, accepted_socket);
            server.serve_one_request(yield);
        });

    // client:
    boost::asio::spawn(io, [&io, &acceptor](boost::asio::yield_context yield)
                       {
                           ip::tcp::socket connecting_socket(io);
                           async_hello_as_a_service_client<ip::tcp::socket, ip::tcp::socket> client(connecting_socket,
                                                                                                    connecting_socket);
                           connecting_socket.async_connect(
                               ip::tcp::endpoint(ip::address_v4::loopback(), acceptor.local_endpoint().port()), yield);
                           std::string name = "Alice";
                           std::string result = client.hello(std::move(name), yield);
                           std::cout << result << '\n';
                       });

    io.run();
}
