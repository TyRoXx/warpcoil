#pragma once

#include <warpcoil/cpp/generate/parser.hpp>
#include <warpcoil/cpp/generate/serialization.hpp>

namespace warpcoil
{
    namespace cpp
    {
        namespace async
        {
            template <class CharSink>
            void generate_serialization_client(CharSink &&code, indentation_level indentation, Si::memory_range name,
                                               types::interface_definition const &definition)
            {
                using Si::append;
                start_line(code, indentation, "template <class AsyncWriteStream, class "
                                              "AsyncReadStream>\nstruct async_");
                append(code, name);
                append(code, "_client");
                append(code, "\n");
                block(
                    code, indentation,
                    [&](indentation_level const in_class)
                    {
                        start_line(code, in_class, "explicit async_");
                        append(code, name);
                        append(code, "_client(AsyncWriteStream &requests, "
                                     "AsyncReadStream "
                                     "&responses)\n");
                        start_line(code, in_class.deeper(), ": requests(requests), "
                                                            "responses(responses), "
                                                            "response_buffer_used(0) {}\n\n");

                        for (auto const &entry : definition.methods)
                        {
                            start_line(code, in_class, "template <class CompletionToken>\n");
                            start_line(code, in_class, "auto ");
                            append(code, entry.first);
                            append(code, "(");
                            type_emptiness const parameter_emptiness =
                                generate_parameters(code, entry.second.parameters);
                            append(code, "CompletionToken &&token)\n");
                            block(
                                code, in_class,
                                [&](indentation_level const in_method)
                                {
                                    switch (parameter_emptiness)
                                    {
                                    case type_emptiness::empty:
                                        for (types::parameter const &parameter_ : entry.second.parameters)
                                        {
                                            start_line(code, in_method, "static_cast<void>(", parameter_.name, ");\n");
                                        }
                                        break;

                                    case type_emptiness::non_empty:
                                        break;
                                    }
                                    start_line(code, in_method, "using handler_type = typename "
                                                                "boost::asio::handler_type<"
                                                                "decltype(token), "
                                                                "void(boost::system::error_code,"
                                                                " ");
                                    type_emptiness const result_emptiness = generate_type(code, entry.second.result);
                                    append(code, ")>::type;\n");
                                    start_line(code, in_method, "handler_type "
                                                                "handler(std::forward<"
                                                                "CompletionToken>(token));\n");
                                    start_line(code, in_method, "boost::asio::async_result<"
                                                                "handler_type> "
                                                                "result(handler);\n");
                                    start_line(code, in_method, "request_buffer.clear();\n");
                                    start_line(code, in_method, "auto request_writer = "
                                                                "Si::Sink<std::uint8_t, "
                                                                "Si::success>::erase(Si::make_"
                                                                "container_sink(request_buffer));"
                                                                "\n");
                                    start_line(code, in_method, "append(request_writer, ");
                                    if (entry.first.size() > 255u)
                                    {
                                        // TODO: avoid this check with a better type for
                                        // the
                                        // name
                                        throw std::invalid_argument("A method name cannot "
                                                                    "be longer than 255 "
                                                                    "bytes");
                                    }
                                    append(code, boost::lexical_cast<std::string>(entry.first.size()));
                                    append(code, "u);\n");
                                    start_line(code, in_method, "append(request_writer, "
                                                                "Si::make_c_str_range(reinterpret_"
                                                                "cast<std::uint8_t const *>(\"");
                                    append(code, entry.first);
                                    append(code, "\")));\n");
                                    for (types::parameter const &param : entry.second.parameters)
                                    {
                                        generate_value_serialization(
                                            code, in_method, Si::make_c_str_range("request_writer"),
                                            Si::make_contiguous_range(param.name), param.type_,
                                            Si::make_c_str_range("handler(warpcoil::cpp::make_invalid_input_error(), "
                                                                 "{}); return result.get()"));
                                    }
                                    switch (result_emptiness)
                                    {
                                    case type_emptiness::non_empty:
                                        start_line(code, in_method,
                                                   "auto receive_buffer = boost::asio::buffer(response_buffer);\n");
                                        start_line(code, in_method,
                                                   "boost::asio::async_write("
                                                   "requests, "
                                                   "boost::asio::buffer(request_"
                                                   "buffer), warpcoil::cpp::request_send_operation<decltype(handler), "
                                                   "AsyncReadStream, decltype(receive_buffer), ");
                                        generate_parser_type(code, entry.second.result);
                                        append(code, ">(std::move(handler), responses, receive_buffer, "
                                                     "response_buffer_used));\n");
                                        break;

                                    case type_emptiness::empty:
                                        start_line(code, in_method, "boost::asio::async_write(requests, "
                                                                    "boost::asio::buffer(request_buffer), "
                                                                    "warpcoil::cpp::no_response_request_send_"
                                                                    "operation<decltype(handler)>(std::move(handler))"
                                                                    ");\n");
                                        break;
                                    }
                                    start_line(code, in_method, "return result.get();\n");
                                },
                                "\n\n");
                        }

                        start_line(code, indentation, "private:\n");
                        start_line(code, in_class, "std::vector<std::uint8_t> request_buffer;\n");
                        start_line(code, in_class, "AsyncWriteStream &requests;\n");
                        start_line(code, in_class, "std::array<std::uint8_t, 512> response_buffer;\n");
                        start_line(code, in_class, "AsyncReadStream &responses;\n");
                        start_line(code, in_class, "std::size_t response_buffer_used;\n");
                    },
                    ";\n\n");
            }
        }
    }
}
