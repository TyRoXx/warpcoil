#pragma once

#include <array>
#include <silicium/exchange.hpp>
#include <warpcoil/protocol.hpp>
#include <warpcoil/cpp/begin_parse_value.hpp>
#include <warpcoil/cpp/integer_parser.hpp>

namespace warpcoil
{
    namespace cpp
    {
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

            template <class RequestHandler>
            void wait_for_request(RequestHandler handler)
            {
                assert(!waiting_for_request);
                waiting_for_request = [handler](boost::system::error_code ec, request_id request) mutable
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
            std::function<void(boost::system::error_code, request_id)> waiting_for_request;
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
                    switch (static_cast<message_type>(type))
                    {
                    case message_type::response:
                        begin_parse_value(pipeline.buffer.input, boost::asio::buffer(pipeline.buffer.response_buffer),
                                          pipeline.buffer.response_buffer_used, integer_parser<request_id>(),
                                          parse_response_operation<DummyHandler>(pipeline, dummy));
                        break;

                    case message_type::request:
                        begin_parse_value(pipeline.buffer.input, boost::asio::buffer(pipeline.buffer.response_buffer),
                                          pipeline.buffer.response_buffer_used, integer_parser<request_id>(),
                                          parse_request_operation<DummyHandler>(pipeline, dummy));
                        break;

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
            struct parse_request_operation
            {
                message_splitter &pipeline;
                DummyHandler dummy;

                explicit parse_request_operation(message_splitter &pipeline, DummyHandler dummy)
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

                    bool const continue_ = pipeline.waiting_for_response != nullptr;

                    assert(pipeline.waiting_for_request);
                    Si::exchange(pipeline.waiting_for_request, nullptr)(ec, request);

                    if (continue_ && !pipeline.parsing_header)
                    {
                        pipeline.begin_parse_message();
                    }
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f, parse_request_operation *operation)
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

                    bool const continue_ = pipeline.waiting_for_request != nullptr;

                    assert(pipeline.waiting_for_response);
                    Si::exchange(pipeline.waiting_for_response, nullptr)(ec, request);

                    if (continue_ && !pipeline.parsing_header)
                    {
                        pipeline.begin_parse_message();
                    }
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
    }
}
