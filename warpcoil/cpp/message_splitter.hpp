#pragma once

#include <array>
#include <silicium/exchange.hpp>
#include <warpcoil/protocol.hpp>
#include <warpcoil/cpp/begin_parse_value.hpp>
#include <warpcoil/cpp/tuple_parser.hpp>
#include <warpcoil/cpp/integer_parser.hpp>
#include <warpcoil/cpp/utf8_parser.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class AsyncReadStream>
        struct buffered_read_stream
        {
            AsyncReadStream &input;
            std::array<std::uint8_t, 512> buffer;
            std::size_t buffer_used;

            explicit buffered_read_stream(AsyncReadStream &input)
                : input(input)
                , buffer_used(0)
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
                waiting_for_response = [handler](boost::system::error_code const ec, request_id const request) mutable
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(
                        [&]
                        {
                            handler(ec, request);
                        },
                        &handler);
                };
                begin_parse_message = nullptr;
                set_begin_parse_message(handler);
                assert(begin_parse_message);
                if (parsing_header)
                {
                    return;
                }
                Si::exchange(begin_parse_message, nullptr)();
            }

            template <class RequestHandler>
            void wait_for_request(RequestHandler handler)
            {
                assert(!waiting_for_request);
                waiting_for_request = [handler](boost::system::error_code const ec, request_id const id,
                                                std::string method) mutable
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(
                        [&]
                        {
                            handler(ec, id, std::move(method));
                        },
                        &handler);
                };
                begin_parse_message = nullptr;
                set_begin_parse_message(handler);
                assert(begin_parse_message);
                if (parsing_header)
                {
                    return;
                }
                Si::exchange(begin_parse_message, nullptr)();
            }

            buffered_read_stream<AsyncReadStream> &lock_input()
            {
                assert(!parsing_header);
                assert(!locked);
                locked = true;
                return buffer;
            }

            void unlock_input()
            {
                assert(!parsing_header);
                assert(locked);
                locked = false;
                if (!waiting_for_response && !waiting_for_request)
                {
                    return;
                }
                assert(begin_parse_message);
                Si::exchange(begin_parse_message, nullptr)();
            }

        private:
            buffered_read_stream<AsyncReadStream> buffer;
            bool locked;
            std::function<void()> begin_parse_message;
            std::function<void(boost::system::error_code, request_id, std::string)> waiting_for_request;
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

                void operator()(boost::system::error_code const ec, message_type_int const type)
                {
                    if (!!ec)
                    {
                        pipeline.on_error(ec);
                        return;
                    }
                    switch (static_cast<message_type>(type))
                    {
                    case message_type::response:
                        if (!pipeline.waiting_for_response)
                        {
                            pipeline.on_error(make_invalid_input_error());
                            return;
                        }
                        begin_parse_value(pipeline.buffer.input, boost::asio::buffer(pipeline.buffer.buffer),
                                          pipeline.buffer.buffer_used, integer_parser<request_id>(),
                                          parse_response_operation<DummyHandler>(pipeline, dummy));
                        break;

                    case message_type::request:
                        if (!pipeline.waiting_for_request)
                        {
                            pipeline.on_error(make_invalid_input_error());
                            return;
                        }
                        begin_parse_value(
                            pipeline.buffer.input, boost::asio::buffer(pipeline.buffer.buffer),
                            pipeline.buffer.buffer_used,
                            tuple_parser<integer_parser<request_id>, utf8_parser<integer_parser<std::uint8_t>>>(),
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

                void operator()(boost::system::error_code const ec, std::tuple<request_id, std::string> request)
                {
                    assert(pipeline.waiting_for_request);
                    if (!!ec)
                    {
                        pipeline.on_error(ec);
                        return;
                    }

                    assert(pipeline.parsing_header);
                    pipeline.parsing_header = false;

                    bool const continue_ = pipeline.waiting_for_response != nullptr;

                    assert(pipeline.waiting_for_request);
                    Si::exchange(pipeline.waiting_for_request, nullptr)(ec, std::get<0>(request),
                                                                        std::move(std::get<1>(request)));

                    if (continue_ && pipeline.waiting_for_response && !pipeline.parsing_header)
                    {
                        assert(pipeline.begin_parse_message);
                        Si::exchange(pipeline.begin_parse_message, nullptr)();
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

                void operator()(boost::system::error_code const ec, request_id const request)
                {
                    assert(pipeline.waiting_for_response);
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

                    if (continue_ && pipeline.waiting_for_request && !pipeline.parsing_header)
                    {
                        assert(pipeline.begin_parse_message);
                        Si::exchange(pipeline.begin_parse_message, nullptr)();
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
                bool const is_waiting_for_request = waiting_for_request != nullptr;
                if (waiting_for_response)
                {
                    Si::exchange(waiting_for_response, nullptr)(ec, 0);
                }
                //"this" might be destroyed at this point if "waiting_for_request" was empty, so we cannot dereference
                //"this" here.
                if (is_waiting_for_request)
                {
                    Si::exchange(waiting_for_request, nullptr)(ec, 0, "");
                }
            }

            template <class DummyHandler>
            void set_begin_parse_message(DummyHandler &handler)
            {
                assert(!begin_parse_message);
                begin_parse_message = [this, handler]()
                {
                    assert(!parsing_header);
                    assert(waiting_for_response || waiting_for_request);
                    if (locked)
                    {
                        return;
                    }
                    parsing_header = true;
                    begin_parse_value(buffer.input, boost::asio::buffer(buffer.buffer), buffer.buffer_used,
                                      integer_parser<message_type_int>(),
                                      parse_message_type_operation<DummyHandler>(*this, handler));
                };
            }
        };
    }
}
