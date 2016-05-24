#pragma once

#include <warpcoil/cpp/message_splitter.hpp>
#include <warpcoil/cpp/wrap_handler.hpp>
#include <map>

namespace warpcoil
{
    namespace cpp
    {
        template <class AsyncReadStream>
        struct expected_response_registry
        {
            explicit expected_response_registry(AsyncReadStream &input)
                : incoming(input)
                , state(response_state::not_expecting_response)
            {
            }

            template <class ResultParser, class ResultHandler>
            void expect_response(request_id const request, ResultHandler handler)
            {
                expected_responses.insert(std::make_pair(
                    request,
                    expected_response{[handler](boost::system::error_code const error) mutable
                                      {
                                          handler(error, typename ResultParser::result_type{});
                                      },
                                      [this, handler]() mutable
                                      {
                                          assert(state == response_state::parsing_result);
                                          buffered_read_stream<AsyncReadStream> &input = incoming.lock_input();
                                          begin_parse_value(
                                              input.input, boost::asio::buffer(input.response_buffer),
                                              input.response_buffer_used, ResultParser(),
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

            std::size_t pending_requests() const
            {
                return expected_responses.size();
            }

            void on_error(boost::system::error_code ec)
            {
                std::map<request_id, expected_response> local_expected_responses = std::move(expected_responses);
                assert(expected_responses.empty());
                state = response_state::not_expecting_response;
                for (auto const &entry : local_expected_responses)
                {
                    entry.second.on_error(ec);
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

            typedef std::tuple<message_type_int, request_id> response_header;

            message_splitter<AsyncReadStream> incoming;
            response_state state;
            std::map<request_id, expected_response> expected_responses;

            template <class DummyHandler>
            void parse_header(DummyHandler &handler)
            {
                state = response_state::parsing_header;
                incoming.wait_for_response(wrap_handler(
                    [this](boost::system::error_code const ec, request_id const request, DummyHandler const &)
                    {
                        assert(state == response_state::parsing_header);
                        if (!!ec)
                        {
                            on_error(ec);
                            return;
                        }
                        auto const entry_found = expected_responses.find(request);
                        if (entry_found == expected_responses.end())
                        {
                            state = response_state::not_expecting_response;
                            throw std::logic_error("TODO handle protocol violation");
                        }
                        std::function<void()> parse_result = std::move(entry_found->second.parse_result);
                        expected_responses.erase(entry_found);
                        state = response_state::parsing_result;
                        parse_result();
                    },
                    handler));
            }

            template <class ResultHandler, class Result>
            struct parse_result_operation
            {
                expected_response_registry &pipeline;
                ResultHandler handler;

                explicit parse_result_operation(expected_response_registry &pipeline, ResultHandler handler)
                    : pipeline(pipeline)
                    , handler(std::move(handler))
                {
                }

                void operator()(boost::system::error_code ec, Result result)
                {
                    assert(pipeline.state == response_state::parsing_result);
                    if (!!ec)
                    {
                        pipeline.on_error(ec);
                        return;
                    }
                    pipeline.incoming.unlock_input();
                    if (pipeline.expected_responses.empty())
                    {
                        pipeline.state = response_state::not_expecting_response;
                    }
                    else
                    {
                        pipeline.parse_header(handler);
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
