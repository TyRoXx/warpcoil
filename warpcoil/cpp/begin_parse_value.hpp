#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/system/error_code.hpp>
#include <warpcoil/cpp/parse_result.hpp>
#include <beast/core/streambuf.hpp>
#include <silicium/optional.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class AsyncReadStream, class Parser, class ResultHandler>
        struct parse_operation
        {
            explicit parse_operation(AsyncReadStream &input, ::beast::streambuf &receive_buffer, Parser parser,
                                     ResultHandler on_result)
                : m_input(input)
                , m_receive_buffer(receive_buffer)
                , m_parser(std::move(parser))
                , m_on_result(std::move(on_result))
            {
            }

            template <bool IsSurelyCalledInHandlerContext>
            void start() &&
            {
                using boost::asio::asio_handler_invoke;
                if (Si::optional<typename Parser::result_type> completed = m_parser.check_for_immediate_completion())
                {
                    if (IsSurelyCalledInHandlerContext)
                    {
                        m_on_result(std::move(*completed));
                    }
                    else
                    {
                        asio_handler_invoke(std::bind(std::ref(m_on_result), std::move(*completed)), &m_on_result);
                    }
                    return;
                }
                std::size_t consumed = 0;
                for (auto const &buffer_piece : m_receive_buffer.data())
                {
                    std::uint8_t const *const data = boost::asio::buffer_cast<std::uint8_t const *>(buffer_piece);
                    for (std::size_t i = 0, c = boost::asio::buffer_size(buffer_piece); i < c; ++i)
                    {
                        parse_result<typename Parser::result_type> response = m_parser.parse_byte(data[i]);
                        if (Si::visit<bool>(
                                response,
                                [&](parse_complete<typename Parser::result_type> &message)
                                {
                                    switch (message.input)
                                    {
                                    case input_consumption::consumed:
                                        ++consumed;
                                        break;

                                    case input_consumption::does_not_consume:
                                        break;
                                    }
                                    m_receive_buffer.consume(consumed);
                                    if (IsSurelyCalledInHandlerContext)
                                    {
                                        m_on_result(std::move(message.result));
                                    }
                                    else
                                    {
                                        asio_handler_invoke(std::bind(std::ref(m_on_result), std::move(message.result)),
                                                            &m_on_result);
                                    }
                                    return true;
                                },
                                [&consumed](need_more_input)
                                {
                                    ++consumed;
                                    return false;
                                },
                                [&](invalid_input)
                                {
                                    if (IsSurelyCalledInHandlerContext)
                                    {
                                        m_on_result(make_invalid_input_error());
                                    }
                                    else
                                    {
                                        asio_handler_invoke(
                                            std::bind(std::ref(m_on_result), make_invalid_input_error()), &m_on_result);
                                    }
                                    return true;
                                }))
                        {
                            return;
                        }
                    }
                }
                m_receive_buffer.consume(consumed);
                m_input.async_read_some(m_receive_buffer.prepare(512), std::move(*this));
            }

            void operator()(boost::system::error_code ec, std::size_t read)
            {
                if (!!ec)
                {
                    // No asio_handler_invoke because we assume that this method gets called only in the correct
                    // context.
                    m_on_result(ec);
                    return;
                }
                m_receive_buffer.commit(read);
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
            ::beast::streambuf &m_receive_buffer;
            Parser m_parser;
            ResultHandler m_on_result;
        };

        template <class AsyncReadStream, class Parser, class ResultHandler>
        void begin_parse_value(AsyncReadStream &input, ::beast::streambuf &receive_buffer, Parser parser,
                               ResultHandler &&on_result)
        {
            parse_operation<AsyncReadStream, Parser, typename std::decay<ResultHandler>::type> operation(
                input, receive_buffer, std::move(parser), std::forward<ResultHandler>(on_result));
            std::move(operation).template start<false>();
        }
    }
}
