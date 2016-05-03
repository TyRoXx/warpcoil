#pragma once

#include <warpcoil/cpp/helpers.hpp>
#include <boost/asio/handler_invoke_hook.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class ResultHandler, class Result>
        struct response_parse_operation
        {
            explicit response_parse_operation(ResultHandler handler)
                : m_handler(std::move(handler))
            {
            }

            void operator()(boost::system::error_code ec, Result result)
            {
                m_handler(ec, std::move(result));
            }

        private:
            ResultHandler m_handler;

            template <class Function>
            friend void asio_handler_invoke(Function &&f, response_parse_operation *operation)
            {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(f, &operation->m_handler);
            }
        };

        template <class ResultHandler, class AsyncReadStream, class Buffer, class Parser>
        struct request_send_operation
        {
            explicit request_send_operation(ResultHandler handler, AsyncReadStream &input, Buffer receive_buffer,
                                            std::size_t &receive_buffer_used)
                : m_handler(std::move(handler))
                , m_input(input)
                , m_receive_buffer(receive_buffer)
                , m_receive_buffer_used(receive_buffer_used)
            {
            }

            void operator()(boost::system::error_code ec, std::size_t)
            {
                if (!!ec)
                {
                    m_handler(ec, {});
                    return;
                }
                begin_parse_value(
                    m_input, m_receive_buffer, m_receive_buffer_used, Parser(),
                    response_parse_operation<ResultHandler, typename Parser::result_type>(std::move(m_handler)));
            }

        private:
            ResultHandler m_handler;
            AsyncReadStream &m_input;
            Buffer m_receive_buffer;
            std::size_t &m_receive_buffer_used;

            template <class Function>
            friend void asio_handler_invoke(Function &&f, request_send_operation *operation)
            {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(f, &operation->m_handler);
            }
        };

        template <class ResultHandler>
        struct no_response_request_send_operation
        {
            explicit no_response_request_send_operation(ResultHandler handler)
                : m_handler(std::move(handler))
            {
            }

            void operator()(boost::system::error_code ec, std::size_t)
            {
                m_handler(ec, {});
            }

        private:
            ResultHandler m_handler;

            template <class Function>
            friend void asio_handler_invoke(Function &&f, no_response_request_send_operation *operation)
            {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(f, &operation->m_handler);
            }
        };
    }
}
