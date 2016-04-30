#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4701 4127 4706 4267 4324)
#endif
#include <beast/websocket.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <silicium/config.hpp>

namespace warpcoil
{
    namespace beast
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
                ::beast::async_completion<CompletionToken, void(boost::system::error_code, std::size_t)> completion(
                    token);
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
                            case ::beast::websocket::opcode::binary:
                            case ::beast::websocket::opcode::text:
                                std::move(handler)(ec, read_from_receive_buffer(buffers));
                                break;

                            case ::beast::websocket::opcode::close:
                                std::move(handler)(websocket_error::close, 0);
                                break;

                            case ::beast::websocket::opcode::cont:
                            case ::beast::websocket::opcode::rsv3:
                            case ::beast::websocket::opcode::rsv4:
                            case ::beast::websocket::opcode::rsv5:
                            case ::beast::websocket::opcode::rsv6:
                            case ::beast::websocket::opcode::rsv7:
                            case ::beast::websocket::opcode::ping:
                            case ::beast::websocket::opcode::pong:
                            case ::beast::websocket::opcode::crsvb:
                            case ::beast::websocket::opcode::crsvc:
                            case ::beast::websocket::opcode::crsvd:
                            case ::beast::websocket::opcode::crsve:
                            case ::beast::websocket::opcode::crsvf:
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
                ::beast::async_completion<CompletionToken, void(boost::system::error_code, std::size_t)> completion(
                    token);
                std::size_t const size = boost::asio::buffer_size(buffers);
                m_websocket.async_write(
                    buffers, write_operation<decltype(completion.handler)>(std::move(completion.handler), size));
                return completion.result.get();
            }

        private:
            WebsocketStream m_websocket;
            ::beast::websocket::opcode m_receivedOpcode;
            ::beast::streambuf m_receiveBuffer;

            template <class Handler>
            struct write_operation
            {
                Handler handler;
                std::size_t bytes_transferred;

                explicit write_operation(Handler handler, std::size_t bytes_transferred)
                    : handler(std::move(handler))
                    , bytes_transferred(bytes_transferred)
                {
                }

                void operator()(boost::system::error_code ec)
                {
                    handler(ec, ec ? 0 : bytes_transferred);
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f, write_operation *operation)
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(f, &operation->handler);
                }
            };

            template <class MutableBufferSequence>
            std::size_t read_from_receive_buffer(MutableBufferSequence buffers)
            {
                std::size_t const copied = boost::asio::buffer_copy(buffers, m_receiveBuffer.data());
                m_receiveBuffer.consume(copied);
                return copied;
            }
        };
    }
}

namespace boost
{
    namespace system
    {
        template <>
        struct is_error_code_enum<warpcoil::beast::websocket_error> : std::true_type
        {
        };
    }
}
