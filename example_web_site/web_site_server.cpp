#include "web_site_interfaces.hpp"
#include "web_site_interface.hpp"
#include <warpcoil/beast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <silicium/error_or.hpp>
#include <iostream>
#include <warpcoil/beast_push_warnings.hpp>
#include <beast/websocket/stream.hpp>
#include <beast/http/read.hpp>
#include <warpcoil/pop_warnings.hpp>
#include <warpcoil/javascript.hpp>
#include <ventura/read_file.hpp>
#include <boost/intrusive/list.hpp>

namespace
{
    struct websocket_client
        : boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
    {
        typedef beast::websocket::stream<boost::asio::ip::tcp::socket &> websocket_type;
        typedef warpcoil::beast::async_stream_adaptor<websocket_type &> adaptor_type;

        boost::asio::ip::tcp::socket socket;
        beast::streambuf receive_buffer;
        websocket_type websocket;
        adaptor_type adaptor;
        warpcoil::cpp::message_splitter<adaptor_type> splitter;
        warpcoil::cpp::buffered_writer<adaptor_type> writer;
        async_publisher_server<async_publisher, adaptor_type, adaptor_type> server_adaptor;
        async_viewer_client<adaptor_type, adaptor_type> viewer_client;

        websocket_client(boost::asio::ip::tcp::socket socket, beast::streambuf receive_buffer,
                         async_publisher &server_impl)
            : socket(std::move(socket))
            , receive_buffer(std::move(receive_buffer))
            , websocket(this->socket)
            , adaptor(this->websocket, beast::streambuf())
            , splitter(this->adaptor)
            , writer(this->adaptor)
            , server_adaptor(server_impl, this->splitter, this->writer)
            , viewer_client(writer, splitter)
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

    struct web_site_publisher : async_publisher
    {
        typedef boost::intrusive::list<websocket_client, boost::intrusive::constant_time_size<false>>
            websocket_client_list;

        websocket_client_list all_clients;

        void publish(std::string message, std::function<void(Si::error_or<std::tuple<>>)> on_result) override
        {
            for (websocket_client &client : all_clients)
            {
                client.viewer_client.display(message, [](Si::error_or<std::tuple<>> result)
                                             {
                                                 result.throw_if_error();
                                             });
            }
            // does not make sense to wait for responses from the clients here
            on_result(std::make_tuple());
        }
    };

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
        beast::http::request_v1<beast::http::string_body> request;

        explicit http_client(boost::asio::ip::tcp::socket socket, beast::streambuf receive_buffer)
            : socket(std::move(socket))
            , receive_buffer(std::move(receive_buffer))
        {
        }
    };

    void begin_serve(std::shared_ptr<http_client> client, web_site_publisher &server_impl,
                     ventura::absolute_path const &document_root);

    void serve_prepared_response(std::shared_ptr<file_client> client, bool const is_keep_alive,
                                 web_site_publisher &server_impl, ventura::absolute_path const &document_root,
                                 boost::string_ref const content_type)
    {
        client->response.version = 11;
        client->response.headers.insert("Content-Length",
                                        boost::lexical_cast<std::string>(client->response.body.size()));
        client->response.headers.insert("Content-Type", content_type);
        beast::http::async_write(client->socket, client->response, [client, is_keep_alive, &server_impl,
                                                                    document_root](boost::system::error_code const ec)
                                 {
                                     Si::throw_if_error(ec);
                                     if (is_keep_alive)
                                     {
                                         auto http_client_again = std::make_shared<http_client>(
                                             std::move(client->socket), std::move(client->receive_buffer));
                                         begin_serve(http_client_again, server_impl, document_root);
                                     }
                                     else
                                     {
                                         client->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                                     }
                                 });
    }

    void serve_static_file(std::shared_ptr<file_client> client, bool const is_keep_alive,
                           web_site_publisher &server_impl, ventura::absolute_path const &document_root,
                           ventura::absolute_path const &served_document, boost::string_ref const content_type)
    {
        Si::visit<void>(ventura::read_file(ventura::safe_c_str(ventura::to_os_string(served_document))),
                        [&](std::vector<char> content)
                        {
                            // TODO: avoid the copy of the content
                            client->response.body.assign(content.begin(), content.end());
                            client->response.reason = "OK";
                            client->response.status = 200;
                        },
                        [&](boost::system::error_code const ec)
                        {
                            std::cerr << "Could not read file " << served_document << ": " << ec << '\n';
                            client->response.reason = "Internal Server Error";
                            client->response.status = 500;
                            client->response.body = client->response.reason;
                        },
                        [&](ventura::read_file_problem const problem)
                        {
                            std::cerr << "Could not read file " << served_document << " due to problem "
                                      << static_cast<int>(problem) << '\n';
                            client->response.reason = "Internal Server Error";
                            client->response.status = 500;
                            client->response.body = client->response.reason;
                        });
        serve_prepared_response(client, is_keep_alive, server_impl, document_root, content_type);
    }

    void serve_interface_library(std::shared_ptr<file_client> client, bool const is_keep_alive,
                                 web_site_publisher &server_impl, ventura::absolute_path const &document_root)
    {
        auto code_writer = Si::Sink<char, Si::success>::erase(Si::make_container_sink(client->response.body));
        Si::append(code_writer, "\"use strict\";\n");

        Si::append(code_writer, "var library = ");
        warpcoil::javascript::generate_common_library(code_writer, warpcoil::indentation_level());
        Si::append(code_writer, ";\n");

        Si::memory_range const library = Si::make_c_str_range("library");
        Si::append(code_writer, "var make_receiver = ");
        warpcoil::javascript::generate_input_receiver(code_writer, warpcoil::indentation_level(),
                                                      warpcoil::create_viewer_interface(), library);
        Si::append(code_writer, ";\n");

        warpcoil::types::interface_definition const service = warpcoil::create_publisher_interface();
        Si::append(code_writer, "var make_client = ");
        warpcoil::javascript::generate_client(code_writer, warpcoil::indentation_level(), service, library);
        Si::append(code_writer, ";\n");

        client->response.reason = "OK";
        client->response.status = 200;
        serve_prepared_response(client, is_keep_alive, server_impl, document_root, "text/javascript");
    }

    void begin_serve(std::shared_ptr<http_client> client, web_site_publisher &server_impl,
                     ventura::absolute_path const &document_root)
    {
        beast::http::async_read(
            client->socket, client->receive_buffer, client->request,
            [client, &server_impl, document_root](boost::system::error_code const ec)
            {
                Si::throw_if_error(ec);
                if (beast::http::is_upgrade(client->request))
                {
                    auto new_client = std::make_shared<websocket_client>(
                        std::move(client->socket), std::move(client->receive_buffer), server_impl);
                    new_client->websocket.async_accept(client->request,
                                                       [new_client, &server_impl](boost::system::error_code const ec)
                                                       {
                                                           Si::throw_if_error(ec);
                                                           server_impl.all_clients.push_back(*new_client);
                                                           begin_serve(new_client);
                                                       });
                }
                else
                {
                    auto new_client =
                        std::make_shared<file_client>(std::move(client->socket), std::move(client->receive_buffer));
                    if (client->request.url == "/index.js")
                    {
                        serve_static_file(new_client, beast::http::is_keep_alive(client->request), server_impl,
                                          document_root, document_root / ventura::relative_path("index.js"),
                                          "text/javascript");
                    }
                    else if (client->request.url == "/service.js")
                    {
                        serve_interface_library(new_client, beast::http::is_keep_alive(client->request), server_impl,
                                                document_root);
                    }
                    else
                    {
                        serve_static_file(new_client, beast::http::is_keep_alive(client->request), server_impl,
                                          document_root, document_root / ventura::relative_path("index.html"),
                                          "text/html");
                    }
                }
            });
    }

    void begin_accept(boost::asio::ip::tcp::acceptor &acceptor, web_site_publisher &server_impl,
                      ventura::absolute_path const &document_root)
    {
        auto client =
            std::make_shared<http_client>(boost::asio::ip::tcp::socket(acceptor.get_io_service()), beast::streambuf());
        acceptor.async_accept(client->socket,
                              [&acceptor, client, &server_impl, document_root](boost::system::error_code const ec)
                              {
                                  Si::throw_if_error(ec);
                                  begin_accept(acceptor, server_impl, document_root);
                                  begin_serve(client, server_impl, document_root);
                              });
    }
}

int main()
{
    using namespace boost::asio;

    web_site_publisher server_impl;
    io_service io;

    boost::uint16_t const port = 8080;

    ventura::absolute_path const document_root = *ventura::parent(*ventura::absolute_path::create(__FILE__));

    ip::tcp::acceptor acceptor_v6(io, ip::tcp::endpoint(ip::tcp::v6(), port), true);
    acceptor_v6.listen();
    begin_accept(acceptor_v6, server_impl, document_root);

    ip::tcp::acceptor acceptor_v4(io, ip::tcp::endpoint(ip::tcp::v4(), port), true);
    acceptor_v4.listen();
    begin_accept(acceptor_v4, server_impl, document_root);

    while (!io.stopped())
    {
        try
        {
            io.run();
        }
        catch (std::exception const &ex)
        {
            std::cerr << "Exception: " << ex.what() << '\n';
            io.reset();
        }
    }
}
