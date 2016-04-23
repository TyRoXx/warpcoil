#include "hello_interfaces.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <silicium/error_or.hpp>
#include <iostream>

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4100 4701 4127 4706 4267 4324)
#endif
#include <beast/websocket.hpp>
#ifdef _MSC_VER
#pragma warning (pop)
#endif

namespace server
{
    enum class websocket_error
    {
        close,
        unexpected_opcode
    };

    struct websocket_error_category : boost::system::error_category
    {
        virtual const char *name() const BOOST_SYSTEM_NOEXCEPT override
        {
            return "websocket_error";
        }

        virtual std::string message(int ev) const override
        {
            switch (static_cast<websocket_error>(ev))
            {
            case websocket_error::close:
                return "close";
            case websocket_error::unexpected_opcode:
                return "unexpected_opcode";
            }
            SILICIUM_UNREACHABLE();
        }
    };

    inline boost::system::error_code make_error_code(websocket_error error)
    {
        static websocket_error_category const category;
        return boost::system::error_code(static_cast<int>(error), category);
    }
}

namespace boost
{
    namespace system
    {
        template <>
        struct is_error_code_enum<server::websocket_error> : std::true_type
        {
        };
    }
}

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

    template <class WebsocketStream>
    struct async_stream_adaptor
    {
        template <class... Args>
        explicit async_stream_adaptor(Args &&... args)
            : m_websocket(std::forward<Args>(args)...)
        {
        }

        WebsocketStream &next_layer()
        {
            return m_websocket;
        }

        WebsocketStream const &next_layer() const
        {
            return m_websocket;
        }

        template <class MutableBufferSequence, class CompletionToken>
        auto async_read_some(MutableBufferSequence buffers, CompletionToken &&token)
        {
            beast::async_completion<CompletionToken, void(boost::system::error_code, std::size_t)> completion(token);
            std::size_t const read = read_from_receive_buffer(buffers);
            if (read)
            {
                std::move(completion.handler)(boost::system::error_code(), read);
            }
            else
            {
                m_websocket.async_read(
                    m_receivedOpcode, m_receiveBuffer,
                    [ this, buffers, handler = std::move(completion.handler) ](boost::system::error_code ec) mutable
                    {
                        if (!!ec)
                        {
                            std::move(handler)(ec, 0);
                            return;
                        }
                        switch (m_receivedOpcode)
                        {
                        case beast::websocket::opcode::binary:
                        case beast::websocket::opcode::text:
                            std::move(handler)(ec, read_from_receive_buffer(buffers));
                            break;

                        case beast::websocket::opcode::close:
                            std::move(handler)(websocket_error::close, 0);
                            break;

                        case beast::websocket::opcode::cont:
                        case beast::websocket::opcode::rsv3:
                        case beast::websocket::opcode::rsv4:
                        case beast::websocket::opcode::rsv5:
                        case beast::websocket::opcode::rsv6:
                        case beast::websocket::opcode::rsv7:
                        case beast::websocket::opcode::ping:
                        case beast::websocket::opcode::pong:
                        case beast::websocket::opcode::crsvb:
                        case beast::websocket::opcode::crsvc:
                        case beast::websocket::opcode::crsvd:
                        case beast::websocket::opcode::crsve:
                        case beast::websocket::opcode::crsvf:
                            std::move(handler)(websocket_error::unexpected_opcode, 0);
                            break;
                        }
                    });
            }
            return completion.result.get();
        }

        template <class ConstBufferSequence, class CompletionToken>
        auto async_write_some(ConstBufferSequence buffers, CompletionToken &&token)
        {
            beast::async_completion<CompletionToken, void(boost::system::error_code, std::size_t)> completion(token);
            std::size_t const size = boost::asio::buffer_size(buffers);
            m_websocket.async_write(
                buffers, [ handler = std::move(completion.handler), size ](boost::system::error_code ec) mutable
                {
                    std::move(handler)(ec, ec ? 0 : size);
                });
            return completion.result.get();
        }

    private:
        WebsocketStream m_websocket;
        beast::websocket::opcode m_receivedOpcode;
        beast::streambuf m_receiveBuffer;

        template <class MutableBufferSequence>
        std::size_t read_from_receive_buffer(MutableBufferSequence buffers)
        {
            std::size_t const copied = boost::asio::buffer_copy(buffers, m_receiveBuffer.data());
            m_receiveBuffer.consume(copied);
            return copied;
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
            typedef beast::websocket::stream<ip::tcp::socket &> websocket;
            auto session = std::make_shared<server::async_stream_adaptor<websocket>>(accepted_socket);
            session->next_layer().async_accept(
                [session, &server_impl](boost::system::error_code ec)
                {
                    Si::throw_if_error(ec);
                    auto server =
                        std::make_shared<async_hello_as_a_service_server<decltype(*session), decltype(*session)>>(
                            server_impl, *session, *session);
                    server->serve_one_request([server, session](boost::system::error_code ec)
                                              {
                                                  Si::throw_if_error(ec);
                                              });
                });
        });

    // client:
    ip::tcp::socket connecting_socket(io);
    connecting_socket.async_connect(
        ip::tcp::endpoint(ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&io, &connecting_socket](boost::system::error_code ec)
        {
            Si::throw_if_error(ec);
            typedef beast::websocket::stream<ip::tcp::socket &> websocket;
            auto session = std::make_shared<server::async_stream_adaptor<websocket>>(connecting_socket);
            session->next_layer().async_handshake(
                "localhost", "/", [session](boost::system::error_code ec)
                {
                    Si::throw_if_error(ec);
                    auto client =
                        std::make_shared<async_hello_as_a_service_client<decltype(*session), decltype(*session)>>(
                            *session, *session);
                    std::string name = "Alice";
                    client->hello(std::move(name), [client](boost::system::error_code ec, std::string result)
                                  {
                                      Si::throw_if_error(ec);
                                      std::cout << result << '\n';
                                  });
                });
        });

    io.run();
}
