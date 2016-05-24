#pragma once

#include <warpcoil/protocol.hpp>
#include <warpcoil/cpp/buffered_writer.hpp>
#include <warpcoil/cpp/integer_parser.hpp>
#include <warpcoil/cpp/tuple_parser.hpp>
#include <warpcoil/cpp/wrap_handler.hpp>
#include <map>
#include <silicium/exchange.hpp>

namespace warpcoil
{
    namespace cpp
    {
        enum class client_pipeline_request_status
        {
            ok,
            failure
        };

        template <class AsyncReadStream>
        struct buffered_read_stream
        {
            AsyncReadStream &input;
            std::array<std::uint8_t, 512> response_buffer;
            std::size_t response_buffer_used;

            explicit buffered_read_stream(AsyncReadStream &input)
                : input(input)
                , response_buffer_used(0)
            {
            }
        };

        template <class AsyncReadStream>
        struct message_splitter
        {
            explicit message_splitter(AsyncReadStream &input)
                : buffer(input)
                , locked(false)
                , parsing_header(false)
            {
            }

            template <class ResponseHandler>
            void wait_for_response(ResponseHandler handler)
            {
                assert(!waiting_for_response);
                waiting_for_response = [handler](boost::system::error_code ec, request_id request) mutable
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(
                        [&]
                        {
                            handler(ec, request);
                        },
                        &handler);
                };
                if (parsing_header)
                {
                    return;
                }
                set_begin_parse_message(handler);
                begin_parse_message();
            }

            buffered_read_stream<AsyncReadStream> &lock_input()
            {
                locked = true;
                return buffer;
            }

            void unlock_input()
            {
                locked = false;
                if (!waiting_for_response)
                {
                    return;
                }
                begin_parse_message();
            }

        private:
            buffered_read_stream<AsyncReadStream> buffer;
            bool locked;
            std::function<void()> begin_parse_message;
            std::function<void(boost::system::error_code, request_id)> waiting_for_response;
            bool parsing_header;

            template <class DummyHandler>
            struct parse_message_type_operation
            {
                message_splitter &pipeline;
                DummyHandler dummy;

                explicit parse_message_type_operation(message_splitter &pipeline, DummyHandler dummy)
                    : pipeline(pipeline)
                    , dummy(std::move(dummy))
                {
                }

                void operator()(boost::system::error_code ec, message_type_int const type)
                {
                    if (!!ec)
                    {
                        pipeline.on_error(ec);
                        return;
                    }
                    switch (type)
                    {
                    case message_type::response:
                        begin_parse_value(pipeline.buffer.input, boost::asio::buffer(pipeline.buffer.response_buffer),
                                          pipeline.buffer.response_buffer_used, integer_parser<request_id>(),
                                          parse_response_operation<DummyHandler>(pipeline, dummy));
                        break;

                    case message_type::request:
                        throw std::logic_error("not implemented");

                    default:
                        pipeline.on_error(make_invalid_input_error());
                        break;
                    }
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f, parse_message_type_operation *operation)
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(f, &operation->dummy);
                }
            };

            template <class DummyHandler>
            struct parse_response_operation
            {
                message_splitter &pipeline;
                DummyHandler dummy;

                explicit parse_response_operation(message_splitter &pipeline, DummyHandler dummy)
                    : pipeline(pipeline)
                    , dummy(std::move(dummy))
                {
                }

                void operator()(boost::system::error_code ec, request_id const request)
                {
                    if (!!ec)
                    {
                        pipeline.on_error(ec);
                        return;
                    }

                    assert(pipeline.parsing_header);
                    pipeline.parsing_header = false;

                    assert(pipeline.waiting_for_response);
                    Si::exchange(pipeline.waiting_for_response, nullptr)(ec, request);
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f, parse_response_operation *operation)
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(f, &operation->dummy);
                }
            };

            void on_error(boost::system::error_code const ec)
            {
                if (waiting_for_response)
                {
                    Si::exchange(waiting_for_response, nullptr)(ec, 0);
                }
            }

            template <class DummyHandler>
            void set_begin_parse_message(DummyHandler &handler)
            {
                begin_parse_message = [this, handler]()
                {
                    assert(!parsing_header);
                    if (locked)
                    {
                        return;
                    }
                    parsing_header = true;
                    begin_parse_value(buffer.input, boost::asio::buffer(buffer.response_buffer),
                                      buffer.response_buffer_used, integer_parser<message_type_int>(),
                                      parse_message_type_operation<DummyHandler>(*this, handler));
                };
            }
        };

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

        template <class AsyncWriteStream, class AsyncReadStream>
        struct client_pipeline
        {
            explicit client_pipeline(AsyncWriteStream &requests, AsyncReadStream &responses)
                : writer(requests)
                , responses(responses)
            {
            }

            std::size_t pending_requests() const
            {
                return responses.pending_requests();
            }

            template <class ResultParser, class RequestBuilder, class ResultHandler>
            void request(RequestBuilder build_request, ResultHandler &handler)
            {
                if (!writer.is_running())
                {
                    writer.async_run([this](boost::system::error_code ec)
                                     {
                                         responses.on_error(ec);
                                     });
                }
                auto sink = Si::Sink<std::uint8_t>::erase(writer.buffer_sink());
                write_integer(sink, static_cast<message_type_int>(message_type::request));
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
                responses.template expect_response<ResultParser>(current_id, handler);
            }

        private:
            buffered_writer<AsyncWriteStream> writer;
            expected_response_registry<AsyncReadStream> responses;
            request_id next_request_id = 0;
        };
    }
}
