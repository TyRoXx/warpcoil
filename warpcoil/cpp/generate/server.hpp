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
                append(code, "template <class Implementation, class AsyncReadStream, class AsyncWriteStream>\n");
                indentation.render(code);
                append(code, "struct async_");
                append(code, name);
                append(code, "_server");
                append(code, "\n");
                block(
                    code, indentation,
                    [&](indentation_level const in_class)
                    {
                        in_class.render(code);
                        append(code, "explicit async_");
                        append(code, name);
                        append(code,
                               "_server(Implementation &implementation, AsyncReadStream &requests, AsyncWriteStream "
                               "&responses)\n");
                        in_class.deeper().render(code);
                        append(code, ": implementation(implementation), requests(requests), "
                                     "request_buffer_used(0), responses(responses) {}\n\n");

                        in_class.render(code);
                        append(code, "template <class "
                                     "CompletionToken>\n");
                        in_class.render(code);
                        append(code, "auto serve_one_request(CompletionToken &&token)\n");
                        block(code, in_class,
                              [&](indentation_level const in_method)
                              {
                                  in_method.render(code);
                                  append(code, "using handler_type = typename "
                                               "boost::asio::handler_type<"
                                               "decltype(token), "
                                               "void(boost::system::error_code)>::type;\n");
                                  in_method.render(code);
                                  append(code, "handler_type "
                                               "handler(std::forward<"
                                               "CompletionToken>(token));\n");
                                  in_method.render(code);
                                  append(code, "boost::asio::async_result<"
                                               "handler_type> "
                                               "result(handler);\n");
                                  in_method.render(code);
                                  append(code, "begin_receive_method_name(std::move(handler));\n");
                                  in_method.render(code);
                                  append(code, "return result.get();\n");
                              },
                              "\n\n");

                        indentation.render(code);
                        append(code, "private:\n");

                        in_class.render(code);
                        append(code, "Implementation &implementation;\n");
                        in_class.render(code);
                        append(code, "AsyncReadStream &requests;\n");
                        in_class.render(code);
                        append(code, "std::array<std::uint8_t, 512> request_buffer;\n");
                        in_class.render(code);
                        append(code, "std::size_t request_buffer_used;\n");
                        in_class.render(code);
                        append(code, "std::vector<std::uint8_t> response_buffer;\n");
                        in_class.render(code);
                        append(code, "AsyncWriteStream &responses;\n\n");
                        in_class.render(code);
                        append(code, "typedef ");
                        generate_parser_type(code, types::utf8{types::integer{0, 255}});
                        append(code, " method_name_parser;\n\n");
                        in_class.render(code);
                        append(code, "template <class Handler>\n");
                        in_class.render(code);
                        append(code, "void begin_receive_method_name(Handler &&handle_result)\n");
                        block(code, in_class,
                              [&](indentation_level const in_method)
                              {
                                  in_method.render(code);
                                  append(code, "begin_parse_value(requests, boost::asio::buffer(request_buffer), "
                                               "request_buffer_used, method_name_parser(), "
                                               "warpcoil::cpp::make_handler_with_argument([this"
                                               "](boost::system::error_code ec, "
                                               "std::string name, Handler &handle_result)\n");
                                  block(code, in_method,
                                        [&](indentation_level const on_result)
                                        {
                                            on_result.render(code);
                                            append(code,
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
                                                append(code, "if (boost::range::equal(name, "
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
                                            on_result.render(code);
                                            append(code, "else { throw std::logic_error(\"to do: handle "
                                                         "unknown method name\"); }\n");
                                        },
                                        ", std::forward<Handler>(handle_result)));\n");
                              },
                              "\n");

                        for (auto const &entry : definition.methods)
                        {
                            append(code, "\n");
                            in_class.render(code);
                            append(code, "template <class Handler>\n");
                            in_class.render(code);
                            append(code, "void begin_receive_method_argument_of_");
                            append(code, entry.first);
                            append(code, "(Handler &&handle_result)\n");
                            block(code, in_class,
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
                                              block(code, on_result,
                                                    [&](indentation_level const in_lambda)
                                                    {
                                                        switch (result_empty)
                                                        {
                                                        case type_emptiness::empty:
                                                            in_lambda.render(code);
                                                            append(code, "std::forward<Handler>(handle_result)(ec);\n");
                                                            break;

                                                        case type_emptiness::non_empty:
                                                            in_lambda.render(code);
                                                            append(code, "if (!!ec) { "
                                                                         "std::forward<Handler>(handle_result)(ec); "
                                                                         "return; }\n");
                                                            in_lambda.render(code);
                                                            append(code, "response_buffer.clear();\n");
                                                            in_lambda.render(code);
                                                            append(code, "auto response_writer = "
                                                                         "Si::Sink<std::uint8_t, "
                                                                         "Si::success>::erase(Si::make_"
                                                                         "container_sink(response_buffer));"
                                                                         "\n");
                                                            generate_value_serialization(
                                                                code, in_lambda,
                                                                Si::make_c_str_range("response_writer"),
                                                                Si::make_c_str_range("result"), entry.second.result,
                                                                Si::make_c_str_range(
                                                                    "return std::forward<Handler>(handle_"
                                                                    "result)(warpcoil::cpp::make_"
                                                                    "invalid_input_error())"));
                                                            in_lambda.render(code);
                                                            append(code, "boost::asio::async_write(responses,"
                                                                         " boost::asio::buffer(response_"
                                                                         "buffer), "
                                                                         "warpcoil::cpp::make_handler_with_argument([]("
                                                                         "boost::"
                                                                         "system::error_code ec, "
                                                                         "std::size_t, Handler &handle_result)\n");
                                                            block(code, in_lambda,
                                                                  [&](indentation_level const in_read)
                                                                  {
                                                                      in_read.render(code);
                                                                      append(code, "std::forward<Handler>("
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
