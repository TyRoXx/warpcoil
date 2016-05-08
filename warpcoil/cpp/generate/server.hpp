#pragma once

#include <warpcoil/cpp/generate/parser.hpp>

namespace warpcoil
{
    namespace cpp
    {
        namespace async
        {
            template <class CharSink>
            void generate_serialization_server(CharSink &&code, indentation_level indentation, Si::memory_range name,
                                               types::interface_definition const &definition)
            {
                using Si::append;
                start_line(code, indentation,
                           "template <class Implementation, class AsyncReadStream, class AsyncWriteStream>\n");
                start_line(code, indentation, "struct async_");
                append(code, name);
                append(code, "_server");
                append(code, "\n");
                block(
                    code, indentation,
                    [&](indentation_level const in_class)
                    {
                        start_line(code, in_class, "explicit async_");
                        append(code, name);
                        append(code,
                               "_server(Implementation &implementation, AsyncReadStream &requests, AsyncWriteStream "
                               "&responses)\n");
                        start_line(code, in_class.deeper(), ": implementation(implementation), requests(requests), "
                                                            "request_buffer_used(0), responses(responses) {}\n\n");

                        start_line(code, in_class, "template <class "
                                                   "CompletionToken>\n");
                        start_line(code, in_class, "auto serve_one_request(CompletionToken &&token)\n");
                        block(code, in_class,
                              [&](indentation_level const in_method)
                              {
                                  start_line(code, in_method, "using handler_type = typename "
                                                              "boost::asio::handler_type<"
                                                              "decltype(token), "
                                                              "void(boost::system::error_code)>::type;\n");
                                  start_line(code, in_method, "handler_type "
                                                              "handler(std::forward<"
                                                              "CompletionToken>(token));\n");
                                  start_line(code, in_method, "boost::asio::async_result<"
                                                              "handler_type> "
                                                              "result(handler);\n");
                                  start_line(code, in_method, "begin_receive_request_header(std::move(handler));\n");
                                  start_line(code, in_method, "return result.get();\n");
                              },
                              "\n\n");

                        start_line(code, indentation, "private:\n");

                        start_line(code, in_class, "Implementation &implementation;\n");
                        start_line(code, in_class, "AsyncReadStream &requests;\n");
                        start_line(code, in_class, "std::array<std::uint8_t, 512> request_buffer;\n");
                        start_line(code, in_class, "std::size_t request_buffer_used;\n");
                        start_line(code, in_class, "std::vector<std::uint8_t> response_buffer;\n");
                        start_line(code, in_class, "AsyncWriteStream &responses;\n\n");
                        start_line(code, in_class, "typedef ");
                        generate_parser_type(
                            code, Si::to_unique(types::tuple{make_vector<types::type>(
                                      types::integer{0, 0xffffffffffffffffu}, types::utf8{types::integer{0, 255}})}));
                        append(code, " request_header_parser;\n\n");
                        start_line(code, in_class, "template <class Handler>\n");
                        start_line(code, in_class, "void begin_receive_request_header(Handler &&handle_result)\n");
                        block(code, in_class,
                              [&](indentation_level const in_method)
                              {
                                  start_line(
                                      code, in_method,
                                      "begin_parse_value(requests, boost::asio::buffer(request_buffer), "
                                      "request_buffer_used, request_header_parser(), "
                                      "warpcoil::cpp::make_handler_with_argument([this"
                                      "](boost::system::error_code ec, "
                                      "std::tuple<request_id, std::string> request_header, Handler &handle_result)\n");
                                  block(code, in_method,
                                        [&](indentation_level const on_result)
                                        {
                                            start_line(
                                                code, on_result,
                                                "if (!!ec) { std::forward<Handler>(handle_result)(ec); return; }\n");
                                            bool first = true;
                                            for (auto const &entry : definition.methods)
                                            {
                                                on_result.render(code);
                                                if (first)
                                                {
                                                    first = false;
                                                }
                                                else
                                                {
                                                    append(code, "else ");
                                                }
                                                append(code, "if (boost::range::equal(std::get<1>(request_header), "
                                                             "Si::make_c_str_range(\"");
                                                append(code, entry.first);
                                                append(code, "\")))\n");
                                                block(code, on_result,
                                                      [&](indentation_level const in_here)
                                                      {
                                                          in_here.render(code);
                                                          append(code, "begin_receive_method_argument_of_");
                                                          append(code, entry.first);
                                                          append(code, "(std::forward<Handler>(handle_result));\n");
                                                      },
                                                      "\n");
                                            }
                                            start_line(code, on_result, "else { throw std::logic_error(\"to do: handle "
                                                                        "unknown method name\"); }\n");
                                        },
                                        ", std::forward<Handler>(handle_result)));\n");
                              },
                              "\n");

                        for (auto const &entry : definition.methods)
                        {
                            append(code, "\n");
                            start_line(code, in_class, "template <class Handler>\n");
                            start_line(code, in_class, "void begin_receive_method_argument_of_");
                            append(code, entry.first);
                            append(code, "(Handler &&handle_result)\n");
                            block(
                                code, in_class,
                                [&](indentation_level const in_method)
                                {
                                    start_line(code, in_method,
                                               "begin_parse_value(requests, boost::asio::buffer(request_buffer), "
                                               "request_buffer_used, ");
                                    types::type const parameter_type = get_parameter_type(entry.second.parameters);
                                    generate_parser_type(code, parameter_type);
                                    append(code, "{}, "
                                                 "warpcoil::cpp::make_handler_with_argument([this](boost::system::"
                                                 "error_code ec, ");
                                    generate_type(code, parameter_type);
                                    append(code, " argument, Handler &handle_result)\n");
                                    block(
                                        code, in_method,
                                        [&](indentation_level const on_result)
                                        {
                                            start_line(
                                                code, in_method,
                                                "if (!!ec) { std::forward<Handler>(handle_result)(ec); return; }\n");
                                            start_line(code, on_result, "implementation.");
                                            append(code, entry.first);
                                            append(code, "(");
                                            move_arguments_out_of_tuple(code, Si::make_c_str_range("argument"),
                                                                        entry.second.parameters.size());
                                            append(code, "warpcoil::cpp::make_handler_with_argument([this](boost::"
                                                         "system::error_code ec, ");
                                            type_emptiness const result_empty =
                                                generate_type(code, entry.second.result);
                                            switch (result_empty)
                                            {
                                            case type_emptiness::empty:
                                                break;

                                            case type_emptiness::non_empty:
                                                append(code, " result");
                                                break;
                                            }
                                            append(code, ", Handler &handle_result)\n");
                                            block(
                                                code, on_result,
                                                [&](indentation_level const in_lambda)
                                                {
                                                    switch (result_empty)
                                                    {
                                                    case type_emptiness::empty:
                                                        start_line(code, in_lambda,
                                                                   "std::forward<Handler>(handle_result)(ec);\n");
                                                        break;

                                                    case type_emptiness::non_empty:
                                                        start_line(code, in_lambda,
                                                                   "if (!!ec) { "
                                                                   "std::forward<Handler>(handle_result)(ec); "
                                                                   "return; }\n");
                                                        start_line(code, in_lambda, "response_buffer.clear();\n");
                                                        start_line(code, in_lambda, "auto response_writer = "
                                                                                    "Si::Sink<std::uint8_t, "
                                                                                    "Si::success>::erase(Si::make_"
                                                                                    "container_sink(response_buffer));"
                                                                                    "\n");
                                                        generate_value_serialization(
                                                            code, in_lambda, Si::make_c_str_range("response_writer"),
                                                            Si::make_c_str_range("result"), entry.second.result,
                                                            Si::make_c_str_range("return std::forward<Handler>(handle_"
                                                                                 "result)(warpcoil::cpp::make_"
                                                                                 "invalid_input_error())"));
                                                        start_line(code, in_lambda,
                                                                   "boost::asio::async_write(responses,"
                                                                   " boost::asio::buffer(response_"
                                                                   "buffer), "
                                                                   "warpcoil::cpp::make_handler_with_argument([]("
                                                                   "boost::"
                                                                   "system::error_code ec, "
                                                                   "std::size_t, Handler &handle_result)\n");
                                                        block(code, in_lambda,
                                                              [&](indentation_level const in_read)
                                                              {
                                                                  start_line(code, in_read, "std::forward<Handler>("
                                                                                            "handle_result)(ec);\n");
                                                              },
                                                              ", std::forward<Handler>(handle_result)));\n");
                                                        break;
                                                    }
                                                },
                                                ", std::forward<Handler>(handle_result)));\n");
                                        },
                                        ", std::forward<Handler>(handle_result)));\n");
                                },
                                "\n");
                        }
                    },
                    ";\n\n");
            }
        }
    }
}
