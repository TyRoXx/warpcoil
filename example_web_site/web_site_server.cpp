#include "web_site_interfaces.hpp"
#include <warpcoil/beast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <silicium/error_or.hpp>
#include <iostream>
#include <warpcoil/beast_push_warnings.hpp>
#include <beast/websocket/stream.hpp>
#include <beast/http/read.hpp>
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

    struct websocket_client
    {
        typedef beast::websocket::stream<boost::asio::ip::tcp::socket &> websocket_type;
        typedef warpcoil::beast::async_stream_adaptor<websocket_type &> adaptor_type;

        boost::asio::ip::tcp::socket socket;
        beast::streambuf receive_buffer;
        websocket_type websocket;
        adaptor_type adaptor;
        warpcoil::cpp::message_splitter<adaptor_type> splitter;
        warpcoil::cpp::buffered_writer<adaptor_type> writer;
        async_web_site_service_server<my_service, adaptor_type, adaptor_type> server_adaptor;

        websocket_client(boost::asio::ip::tcp::socket socket, beast::streambuf receive_buffer, my_service &server_impl)
            : socket(std::move(socket))
            , receive_buffer(std::move(receive_buffer))
            , websocket(this->socket)
            , adaptor(this->websocket)
            , splitter(this->adaptor)
            , writer(this->adaptor)
            , server_adaptor(server_impl, this->splitter, this->writer)
        {
        }
    };

    void begin_serve(std::shared_ptr<websocket_client> client)
    {
        client->server_adaptor.serve_one_request([client](boost::system::error_code const ec)
                                                 {
                                                     Si::throw_if_error(ec);
                                                     begin_serve(client);
                                                 });
    }

    struct file_client
    {
        boost::asio::ip::tcp::socket socket;
        beast::streambuf receive_buffer;
        beast::http::response_v1<beast::http::string_body> response;

        explicit file_client(boost::asio::ip::tcp::socket socket, beast::streambuf receive_buffer)
            : socket(std::move(socket))
            , receive_buffer(std::move(receive_buffer))
        {
        }
    };

    struct http_client
    {
        boost::asio::ip::tcp::socket socket;
        beast::streambuf receive_buffer;
        beast::http::request_v1<beast::http::empty_body> request;

        explicit http_client(boost::asio::ip::tcp::socket socket, beast::streambuf receive_buffer)
            : socket(std::move(socket))
            , receive_buffer(std::move(receive_buffer))
        {
        }
    };

    void begin_serve(std::shared_ptr<http_client> client, my_service &server_impl)
    {
        beast::http::async_read(
            client->socket, client->receive_buffer, client->request,
            [client, &server_impl](boost::system::error_code const ec)
            {
                Si::throw_if_error(ec);
                if (beast::http::is_upgrade(client->request))
                {
                    auto new_client = std::make_shared<websocket_client>(
                        std::move(client->socket), std::move(client->receive_buffer), server_impl);
                    new_client->websocket.async_accept(client->request, [new_client](boost::system::error_code const ec)
                                                       {
                                                           Si::throw_if_error(ec);
                                                           begin_serve(new_client);
                                                       });
                }
                else
                {
                    auto new_client =
                        std::make_shared<file_client>(std::move(client->socket), std::move(client->receive_buffer));
                    new_client->response.version = 11;
                    new_client->response.body = "Hello!";
                    new_client->response.reason = "OK";
                    new_client->response.status = 200;
                    new_client->response.headers.insert(
                        "Content-Length", boost::lexical_cast<std::string>(new_client->response.body.size()));
                    new_client->response.headers.insert("Content-Type", "text/html");
                    bool keep_alive = beast::http::is_keep_alive(client->request);
                    beast::http::async_write(
                        new_client->socket, new_client->response,
                        [new_client, keep_alive, &server_impl](boost::system::error_code const ec)
                        {
                            Si::throw_if_error(ec);
                            if (keep_alive)
                            {
                                auto http_client_again = std::make_shared<http_client>(
                                    std::move(new_client->socket), std::move(new_client->receive_buffer));
                                begin_serve(http_client_again, server_impl);
                            }
                            else
                            {
                                new_client->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                            }
                        });
                }
            });
    }

    void begin_accept(boost::asio::ip::tcp::acceptor &acceptor, my_service &server_impl)
    {
        auto client =
            std::make_shared<http_client>(boost::asio::ip::tcp::socket(acceptor.get_io_service()), beast::streambuf());
        acceptor.async_accept(client->socket, [&acceptor, client, &server_impl](boost::system::error_code const ec)
                              {
                                  Si::throw_if_error(ec);
                                  begin_accept(acceptor, server_impl);
                                  begin_serve(client, server_impl);
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
