#pragma once

#include <warpcoil/cpp/generate/generate_type.hpp>
#include <warpcoil/cpp/generate/generate_parser_type.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink1, class CharSink2>
        void generate_serialization_server(CharSink1 &&code,
                                           shared_code_generator<CharSink2> &shared,
                                           indentation_level indentation, Si::memory_range name,
                                           types::interface_definition const &definition)
        {
            using Si::append;
            start_line(code, indentation, "template <class Implementation, "
                                          "class AsyncReadStream, class "
                                          "AsyncWriteStream>\n");
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
                    append(code, "_server(Implementation &implementation, "
                                 "warpcoil::cpp::message_splitter<AsyncReadStream> "
                                 "&requests, "
                                 "warpcoil::cpp::buffered_writer<AsyncWriteStream> "
                                 "&responses)\n");
                    start_line(code, in_class.deeper(),
                               ": implementation(implementation), requests(requests), "
                               "responses(responses) {}\n\n");

                    start_line(code, in_class, "template <class "
                                               "CompletionToken>\n");
                    start_line(code, in_class, "auto serve_one_request(CompletionToken &&token)\n");
                    block(code, in_class,
                          [&](indentation_level const in_method)
                          {
                              start_line(code, in_method,
                                         "using handler_type = typename "
                                         "boost::asio::handler_type<"
                                         "decltype(token), "
                                         "void(boost::system::error_code)>::type;\n");
                              start_line(code, in_method, "handler_type "
                                                          "handler(std::forward<"
                                                          "CompletionToken>(token));\n");
                              start_line(code, in_method, "boost::asio::async_result<"
                                                          "handler_type> "
                                                          "result(handler);\n");
                              start_line(code, in_method, "begin_receive_"
                                                          "request_header(std::"
                                                          "move(handler));\n");
                              start_line(code, in_method, "return result.get();\n");
                          },
                          "\n\n");

                    start_line(code, indentation, "private:\n");
                    start_line(code, in_class, "Implementation &implementation;\n");
                    start_line(code, in_class, "warpcoil::cpp::message_"
                                               "splitter<AsyncReadStream> "
                                               "&requests;\n");
                    start_line(code, in_class, "warpcoil::cpp::buffered_writer<"
                                               "AsyncWriteStream> "
                                               "&responses;\n\n");
                    start_line(code, in_class, "template <class Handler>\n");
                    start_line(code, in_class, "void "
                                               "begin_receive_request_header("
                                               "Handler &&handle_result)\n");
                    block(code, in_class,
                          [&](indentation_level const in_method)
                          {
                              start_line(code, in_method, "requests.wait_for_request(warpcoil::"
                                                          "cpp::wrap_handler([this]"
                                                          "(Si::error_or<std::tuple<warpcoil::"
                                                          "request_id, "
                                                          "std::string>> const &request, Handler "
                                                          "&handle_result)\n");
                              block(code, in_method,
                                    [&](indentation_level const on_result)
                                    {
                                        start_line(code, on_result, "if (request.is_error()) { "
                                                                    "std::forward<Handler>(handle_"
                                                                    "result)(request."
                                                                    "error()); return; }\n");
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
                                            append(code, "if "
                                                         "(boost::range::equal(std:"
                                                         ":get<1>(request.get()), "
                                                         "Si::make_c_str_range(\"");
                                            append(code, entry.first);
                                            append(code, "\")))\n");
                                            block(code, on_result,
                                                  [&](indentation_level const in_here)
                                                  {
                                                      in_here.render(code);
                                                      append(code, "begin_receive_"
                                                                   "method_argument_"
                                                                   "of_");
                                                      append(code, entry.first);
                                                      append(code, "(requests.lock_input(),"
                                                                   " std::get<0>(request."
                                                                   "get()), "
                                                                   "std::forward<Handler>("
                                                                   "handle_result));\n");
                                                  },
                                                  "\n");
                                        }
                                        start_line(code, on_result, "else { throw "
                                                                    "std::logic_error(\"to do: "
                                                                    "handle "
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
                        append(code, "(warpcoil::cpp::buffered_read_stream<"
                                     "AsyncReadStream> &input, "
                                     "warpcoil::request_id const "
                                     "request_id, Handler &&handle_result)\n");
                        block(code, in_class,
                              [&](indentation_level const in_method)
                              {
                                  start_line(code, in_method, "begin_parse_value("
                                                              "input.input, "
                                                              "input.buffer, ");
                                  types::type const parameter_type =
                                      get_parameter_type(entry.second.parameters);
                                  generate_parser_type(code, parameter_type);
                                  append(code, "{}, "
                                               "warpcoil::cpp::wrap_handler(["
                                               "this, request_id](Si::error_or<");
                                  generate_type(code, shared, parameter_type);
                                  append(code, "> argument, Handler &handle_result)\n");
                                  block(
                                      code, in_method,
                                      [&](indentation_level const on_result)
                                      {
                                          start_line(code, in_method, "requests.unlock_input();\n");
                                          start_line(code, in_method, "if (argument.is_error()) { "
                                                                      "std::forward<Handler>("
                                                                      "handle_result)(argument."
                                                                      "error()); return; }\n");
                                          start_line(code, on_result, "implementation.");
                                          append(code, entry.first);
                                          append(code, "(");
                                          move_arguments_out_of_tuple(
                                              code, Si::make_c_str_range("argument.get()"),
                                              entry.second.parameters.size());
                                          append(code, "warpcoil::cpp::wrap_"
                                                       "handler([this, "
                                                       "request_id](Si::error_"
                                                       "or<");
                                          type_emptiness const result_empty =
                                              generate_type(code, shared, entry.second.result);
                                          append(code, "> result");
                                          append(code, ", Handler &handle_result)\n");
                                          block(
                                              code, on_result,
                                              [&](indentation_level const in_lambda)
                                              {
                                                  start_line(code, in_lambda,
                                                             "if (result.is_error()) { "
                                                             "std::forward<Handler>("
                                                             "handle_result)(result."
                                                             "error()); "
                                                             "return; }\n");
                                                  start_line(code, in_lambda,
                                                             "auto response_writer = "
                                                             "Si::Sink<std::uint8_t, "
                                                             "Si::success>::erase("
                                                             "responses.buffer_sink());"
                                                             "\n");
                                                  start_line(code, in_lambda,
                                                             "warpcoil::cpp::write_"
                                                             "integer(response_"
                                                             "writer, "
                                                             "static_cast<warpcoil::"
                                                             "message_type_int>("
                                                             "warpcoil::message_type::"
                                                             "response));\n");
                                                  start_line(code, in_lambda, "warpcoil::cpp::"
                                                                              "write_integer("
                                                                              "response_writer, "
                                                                              "request_id);\n");
                                                  switch (result_empty)
                                                  {
                                                  case type_emptiness::empty:
                                                      break;

                                                  case type_emptiness::non_empty:
                                                      generate_value_serialization(
                                                          code, shared, in_lambda,
                                                          Si::make_c_str_range("response_writer"),
                                                          Si::make_c_str_range("result.get()"),
                                                          entry.second.result,
                                                          Si::make_c_str_range("return "
                                                                               "std::forward<"
                                                                               "Handler>(handle_"
                                                                               "result)(warpcoil::"
                                                                               "cpp::make_"
                                                                               "invalid_input_"
                                                                               "error())"));
                                                      break;
                                                  }
                                                  start_line(code, in_lambda,
                                                             "responses.send_buffer("
                                                             "warpcoil::cpp::wrap_"
                                                             "handler([]("
                                                             "boost::system::error_code "
                                                             "ec, Handler "
                                                             "&handle_result)\n");
                                                  block(code, in_lambda,
                                                        [&](indentation_level const in_read)
                                                        {
                                                            start_line(code, in_read,
                                                                       "std::forward<"
                                                                       "Handler>("
                                                                       "handle_result)(ec)"
                                                                       ";\n");
                                                        },
                                                        ", "
                                                        "std::forward<Handler>("
                                                        "handle_result)));\n");
                                              },
                                              ", "
                                              "std::forward<Handler>(handle_"
                                              "result)));\n");
                                      },
                                      ", "
                                      "std::forward<Handler>(handle_result)));"
                                      "\n");
                              },
                              "\n");
                    }
                },
                ";\n\n");
        }
    }
}
