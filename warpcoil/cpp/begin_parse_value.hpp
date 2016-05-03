#pragma once

#include <warpcoil/cpp/helpers.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/handler_invoke_hook.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class AsyncReadStream, class Buffer, class Parser, class ResultHandler>
        struct parse_operation
        {
            explicit parse_operation(AsyncReadStream &input, Buffer receive_buffer, std::size_t &receive_buffer_used,
                                     Parser parser, ResultHandler on_result)
                : m_input(input)
                , m_receive_buffer(receive_buffer)
                , m_receive_buffer_used(receive_buffer_used)
                , m_parser(std::move(parser))
                , m_on_result(std::move(on_result))
            {
            }

            template <bool IsSurelyCalledInHandlerContext>
            void start() &&
            {
                using boost::asio::asio_handler_invoke;
                for (std::size_t i = 0; i < m_receive_buffer_used; ++i)
                {
                    std::uint8_t *const data = boost::asio::buffer_cast<std::uint8_t *>(m_receive_buffer);
                    parse_result<typename Parser::result_type> response = m_parser.parse_byte(data[i]);
                    if (Si::visit<bool>(
                            response,
                            [&](typename Parser::result_type &message)
                            {
                                std::copy(data + i + 1, data + m_receive_buffer_used, data);
                                m_receive_buffer_used -= 1 + i;
                                if (IsSurelyCalledInHandlerContext)
                                {
                                    m_on_result(boost::system::error_code(), std::move(message));
                                }
                                else
                                {
                                    asio_handler_invoke(std::bind(std::ref(m_on_result), boost::system::error_code(),
                                                                  std::move(message)),
                                                        &m_on_result);
                                }
                                return true;
                            },
                            [](need_more_input)
                            {
                                return false;
                            },
                            [&](invalid_input)
                            {
                                if (IsSurelyCalledInHandlerContext)
                                {
                                    m_on_result(make_invalid_input_error(), typename Parser::result_type{});
                                }
                                else
                                {
                                    asio_handler_invoke(std::bind(std::ref(m_on_result), make_invalid_input_error(),
                                                                  typename Parser::result_type{}),
                                                        &m_on_result);
                                }
                                return true;
                            }))
                    {
                        return;
                    }
                }
                m_input.async_read_some(m_receive_buffer, std::move(*this));
            }

            void operator()(boost::system::error_code ec, std::size_t read)
            {
                if (!!ec)
                {
                    // No asio_handler_invoke because we assume that this method gets called only in the correct
                    // context.
                    m_on_result(ec, typename Parser::result_type{});
                    return;
                }
                m_receive_buffer_used = read;
                std::move(*this).template start<true>();
            }

            template <class Function>
            friend void asio_handler_invoke(Function &&f, parse_operation *operation)
            {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(f, &operation->m_on_result);
            }

        private:
            AsyncReadStream &m_input;
            Buffer m_receive_buffer;
            std::size_t &m_receive_buffer_used;
            Parser m_parser;
            ResultHandler m_on_result;
        };

        template <class AsyncReadStream, class Buffer, class Parser, class ResultHandler>
        void begin_parse_value(AsyncReadStream &input, Buffer receive_buffer, std::size_t &receive_buffer_used,
                               Parser parser, ResultHandler &&on_result)
        {
            parse_operation<AsyncReadStream, Buffer, Parser, typename std::decay<ResultHandler>::type> operation(
                input, receive_buffer, receive_buffer_used, std::move(parser), std::forward<ResultHandler>(on_result));
            std::move(operation).template start<false>();
        }
    }
}
