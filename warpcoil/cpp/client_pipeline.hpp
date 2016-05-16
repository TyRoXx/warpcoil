#pragma once

#include <warpcoil/cpp/buffered_writer.hpp>

namespace warpcoil
{
    namespace cpp
    {
        namespace detail
        {
            template <class ResultHandler, class Result>
            struct response_parse_operation
            {
                explicit response_parse_operation(ResultHandler handler)
                    : m_handler(std::move(handler))
                {
                }

                void operator()(boost::system::error_code ec, std::tuple<request_id, Result> response)
                {
                    // TODO: use request_id
                    m_handler(ec, std::get<1>(std::move(response)));
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
        }

        enum class client_pipeline_request_status
        {
            ok,
            failure
        };

        template <class AsyncWriteStream, class AsyncReadStream>
        struct client_pipeline
        {
            explicit client_pipeline(AsyncWriteStream &requests, AsyncReadStream &responses)
                : writer(requests)
                , responses(responses)
                , response_buffer_used(0)
            {
                writer.async_run([](boost::system::error_code)
                                 {
                                     // Can be ignored because begin_parse_value will fail if the connection goes away.
                                 });
            }

            template <class ResultParser, class RequestBuilder, class ResultHandler>
            void request(RequestBuilder build_request, ResultHandler &handler)
            {
                auto sink = Si::Sink<std::uint8_t>::erase(writer.buffer_sink());
                write_integer(sink, next_request_id);
                ++next_request_id;
                {
                    switch (build_request(sink))
                    {
                    case client_pipeline_request_status::ok:
                        break;

                    case client_pipeline_request_status::failure:
                        return;
                    }
                }
                writer.send_buffer();
                begin_parse_value(
                    responses, boost::asio::buffer(response_buffer), response_buffer_used,
                    tuple_parser<integer_parser<request_id>, ResultParser>(),
                    detail::response_parse_operation<typename std::decay<ResultHandler>::type,
                                                     typename ResultParser::result_type>(std::move(handler)));
            }

        private:
            buffered_writer<AsyncWriteStream> writer;
            std::array<std::uint8_t, 512> response_buffer;
            AsyncReadStream &responses;
            std::size_t response_buffer_used;
            request_id next_request_id = 0;
        };
    }
}