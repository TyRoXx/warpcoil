#pragma once

#include <array>
#include <silicium/exchange.hpp>
#include <warpcoil/protocol.hpp>
#include <warpcoil/cpp/begin_parse_value.hpp>
#include <warpcoil/cpp/tuple_parser.hpp>
#include <warpcoil/cpp/integer_parser.hpp>
#include <warpcoil/cpp/utf8_parser.hpp>
#include <silicium/error_or.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class AsyncReadStream>
        struct buffered_read_stream
        {
            AsyncReadStream &input;
            ::beast::streambuf buffer;

            explicit buffered_read_stream(AsyncReadStream &input)
                : input(input)
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
                waiting_for_response = [handler](Si::error_or<request_id> const request) mutable
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(
                        [&]
                        {
                            handler(request);
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
                begin_parse_message();
            }

            template <class RequestHandler>
            void wait_for_request(RequestHandler handler)
            {
                assert(!waiting_for_request);
                waiting_for_request =
                    [handler](Si::error_or<std::tuple<request_id, std::string>> request) mutable
                {
                    using boost::asio::asio_handler_invoke;
                    asio_handler_invoke(
                        [&]
                        {
                            handler(std::move(request));
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
                begin_parse_message();
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
                begin_parse_message();
            }

        private:
            buffered_read_stream<AsyncReadStream> buffer;
            bool locked;
            std::function<void()> begin_parse_message;
            std::function<void(Si::error_or<std::tuple<request_id, std::string>>)>
                waiting_for_request;
            std::function<void(Si::error_or<request_id>)> waiting_for_response;
            bool parsing_header;

            template <class DummyHandler>
            struct parse_message_type_operation
            {
                message_splitter &pipeline;
                DummyHandler dummy;

                explicit parse_message_type_operation(message_splitter &pipeline,
                                                      DummyHandler dummy)
                    : pipeline(pipeline)
                    , dummy(std::move(dummy))
                {
                }

                void operator()(Si::error_or<message_type_int> const type)
                {
                    if (type.is_error())
                    {
                        pipeline.on_error(type.error());
                        return;
                    }
                    switch (static_cast<message_type>(type.get()))
                    {
                    case message_type::response:
                        if (!pipeline.waiting_for_response)
                        {
                            pipeline.on_error(make_invalid_input_error());
                            return;
                        }
                        begin_parse_value(pipeline.buffer.input, pipeline.buffer.buffer,
                                          integer_parser<request_id>(),
                                          parse_response_operation<DummyHandler>(pipeline, dummy));
                        break;

                    case message_type::request:
                        if (!pipeline.waiting_for_request)
                        {
                            pipeline.on_error(make_invalid_input_error());
                            return;
                        }
                        begin_parse_value(pipeline.buffer.input, pipeline.buffer.buffer,
                                          tuple_parser<integer_parser<request_id>,
                                                       utf8_parser<integer_parser<std::uint8_t>>>(),
                                          parse_request_operation<DummyHandler>(pipeline, dummy));
                        break;

                    default:
                        pipeline.on_error(make_invalid_input_error());
                        break;
                    }
                }

                template <class Function>
                friend void asio_handler_invoke(Function &&f,
                                                parse_message_type_operation *operation)
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

                void operator()(Si::error_or<std::tuple<request_id, std::string>> request)
                {
                    assert(pipeline.waiting_for_request);
                    if (request.is_error())
                    {
                        pipeline.on_error(request.error());
                        return;
                    }

                    assert(pipeline.parsing_header);
                    pipeline.parsing_header = false;

                    bool const continue_ = pipeline.waiting_for_response != nullptr;

                    assert(pipeline.waiting_for_request);
                    Si::exchange(pipeline.waiting_for_request, nullptr)(std::move(request.get()));

                    if (continue_ && pipeline.waiting_for_response && !pipeline.parsing_header)
                    {
                        assert(pipeline.begin_parse_message);
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

                void operator()(Si::error_or<request_id> const request)
                {
                    assert(pipeline.waiting_for_response);
                    if (request.is_error())
                    {
                        pipeline.on_error(request.error());
                        return;
                    }

                    assert(pipeline.parsing_header);
                    pipeline.parsing_header = false;

                    bool const continue_ = pipeline.waiting_for_request != nullptr;

                    assert(pipeline.waiting_for_response);
                    Si::exchange(pipeline.waiting_for_response, nullptr)(request);

                    if (continue_ && pipeline.waiting_for_request && !pipeline.parsing_header)
                    {
                        assert(pipeline.begin_parse_message);
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
                bool const is_waiting_for_request = waiting_for_request != nullptr;
                if (waiting_for_response)
                {
                    Si::exchange(waiting_for_response, nullptr)(ec);
                }
                //"this" might be destroyed at this point if
                //"waiting_for_request" was empty, so we cannot dereference
                //"this" here.
                if (is_waiting_for_request)
                {
                    Si::exchange(waiting_for_request, nullptr)(ec);
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

                    auto const begin_parse_message_keepalive = std::move(begin_parse_message);
                    begin_parse_message = nullptr;

                    parsing_header = true;
                    begin_parse_value(buffer.input, buffer.buffer,
                                      integer_parser<message_type_int>(),
                                      parse_message_type_operation<DummyHandler>(*this, handler));
                };
            }
        };
    }
}
