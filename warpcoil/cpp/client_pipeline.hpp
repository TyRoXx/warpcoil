#pragma once

#include <warpcoil/cpp/buffered_writer.hpp>
#include <map>

namespace warpcoil
{
    namespace cpp
    {
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
                , state(response_state::not_expecting_response)
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
                request_id const current_id = next_request_id;
                ++next_request_id;
                write_integer(sink, current_id);
                switch (build_request(sink))
                {
                case client_pipeline_request_status::ok:
                    break;

                case client_pipeline_request_status::failure:
                    return;
                }
                writer.send_buffer();
                expected_responses.insert(std::make_pair(
                    current_id,
                    expected_response{[handler](boost::system::error_code const error) mutable
                                      {
                                          handler(error, {});
                                      },
                                      [this, handler]() mutable
                                      {
                                          assert(state == response_state::parsing_result);
                                          begin_parse_value(
                                              responses, boost::asio::buffer(response_buffer), response_buffer_used,
                                              ResultParser(),
                                              parse_result_operation<ResultHandler, typename ResultParser::result_type>(
                                                  *this, std::move(handler)));
                                      }}));
                switch (state)
                {
                case response_state::not_expecting_response:
                    parse_header(handler);
                    break;

                case response_state::parsing_header:
                case response_state::parsing_result:
                    break;
                }
            }

        private:
            enum response_state
            {
                not_expecting_response,
                parsing_header,
                parsing_result
            };

            struct expected_response
            {
                std::function<void(boost::system::error_code)> on_error;
                std::function<void()> parse_result;
            };

            buffered_writer<AsyncWriteStream> writer;
            std::array<std::uint8_t, 512> response_buffer;
            AsyncReadStream &responses;
            std::size_t response_buffer_used;
            request_id next_request_id = 0;
            response_state state;
            std::map<request_id, expected_response> expected_responses;

            template <class DummyHandler>
            void parse_header(DummyHandler &handler)
            {
                begin_parse_value(responses, boost::asio::buffer(response_buffer), response_buffer_used,
                                  integer_parser<request_id>(), parse_header_operation<DummyHandler>(*this, handler));
                state = response_state::parsing_header;
            }

            template <class DummyHandler>
            struct parse_header_operation
            {
                client_pipeline &pipeline;
                DummyHandler handler;

                explicit parse_header_operation(client_pipeline &pipeline, DummyHandler handler)
                    : pipeline(pipeline)
                    , handler(std::move(handler))
                {
                }

                void operator()(boost::system::error_code ec, request_id const id) const
                {
                    assert(pipeline.state == response_state::parsing_header);
                    if (!!ec)
                    {
                        std::map<request_id, expected_response> local_expected_responses =
                            std::move(pipeline.expected_responses);
                        assert(pipeline.expected_responses.empty());
                        pipeline.state = response_state::not_expecting_response;
                        for (auto const &entry : local_expected_responses)
                        {
                            entry.second.on_error(ec);
                        }
                        return;
                    }
                    auto const entry_found = pipeline.expected_responses.find(id);
                    if (entry_found == pipeline.expected_responses.end())
                    {
                        pipeline.state = response_state::not_expecting_response;
                        throw std::logic_error("TODO handle protocol violation");
                    }
                    std::function<void()> parse_result = std::move(entry_found->second.parse_result);
                    pipeline.expected_responses.erase(entry_found);
                    pipeline.state = response_state::parsing_result;
                    parse_result();
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f, parse_header_operation *operation)
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(f, &operation->handler);
                }
            };

            template <class ResultHandler, class Result>
            struct parse_result_operation
            {
                client_pipeline &pipeline;
                ResultHandler handler;

                explicit parse_result_operation(client_pipeline &pipeline, ResultHandler handler)
                    : pipeline(pipeline)
                    , handler(std::move(handler))
                {
                }

                void operator()(boost::system::error_code ec, Result result)
                {
                    assert(pipeline.state == response_state::parsing_result);
                    if (!!ec)
                    {
                        pipeline.state = response_state::not_expecting_response;
                    }
                    else
                    {
                        if (pipeline.expected_responses.empty())
                        {
                            pipeline.state = response_state::not_expecting_response;
                        }
                        else
                        {
                            pipeline.parse_header(handler);
                            assert(pipeline.state == response_state::parsing_header);
                        }
                    }
                    handler(ec, std::move(result));
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f, parse_result_operation *operation)
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(f, &operation->handler);
                }
            };
        };
    }
}
