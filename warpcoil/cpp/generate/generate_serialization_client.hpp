#pragma once

#include <warpcoil/cpp/generate/generate_parameters.hpp>
#include <warpcoil/cpp/generate/generate_value_serialization.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink1, class CharSink2>
        void generate_serialization_client(CharSink1 &&code,
                                           shared_code_generator<CharSink2> &shared,
                                           indentation_level indentation, Si::memory_range name,
                                           types::interface_definition const &definition)
        {
            using Si::append;
            start_line(code, indentation, "template <class AsyncWriteStream, class "
                                          "AsyncReadStream>\nstruct async_");
            append(code, name);
            append(code, "_client");
            append(code, "\n");
            block(code, indentation,
                  [&](indentation_level const in_class)
                  {
                      start_line(code, in_class, "explicit async_");
                      append(code, name);
                      append(code, "_client(warpcoil::cpp::buffered_writer<"
                                   "AsyncWriteStream> &requests, "
                                   "warpcoil::cpp::message_splitter<AsyncReadStream> "
                                   "&responses)\n");
                      start_line(code, in_class.deeper(), ": pipeline(requests, responses) {}\n\n");

                      for (auto const &entry : definition.methods)
                      {
                          start_line(code, in_class, "template <class CompletionToken>\n");
                          start_line(code, in_class, "auto ");
                          append(code, entry.first);
                          append(code, "(");
                          type_emptiness const parameter_emptiness =
                              generate_parameters(code, shared, entry.second.parameters);
                          append(code, "CompletionToken &&token)\n");
                          block(code, in_class,
                                [&](indentation_level const in_method)
                                {
                                    switch (parameter_emptiness)
                                    {
                                    case type_emptiness::empty:
                                        for (types::parameter const &parameter_ :
                                             entry.second.parameters)
                                        {
                                            start_line(code, in_method, "static_cast<void>(",
                                                       parameter_.name, ");\n");
                                        }
                                        break;

                                    case type_emptiness::non_empty:
                                        break;
                                    }
                                    start_line(code, in_method, "using handler_type = typename "
                                                                "boost::asio::handler_type<"
                                                                "decltype(token), "
                                                                "void(Si::error_or<"
                                                                " ");
                                    generate_type(code, shared, entry.second.result);
                                    append(code, ">)>::type;\n");
                                    start_line(code, in_method, "handler_type "
                                                                "handler(std::forward<"
                                                                "CompletionToken>(token));\n");
                                    start_line(code, in_method, "boost::asio::async_result<"
                                                                "handler_type> "
                                                                "result(handler);\n");
                                    start_line(code, in_method, "pipeline.template request<");
                                    generate_parser_type(code, entry.second.result);
                                    append(code, ">([&](Si::Sink<std::uint8_t>::"
                                                 "interface &request_writer) -> "
                                                 "warpcoil::cpp::client_pipeline_"
                                                 "request_status\n");
                                    block(
                                        code, in_method,
                                        [&](indentation_level const in_builder)
                                        {
                                            start_line(code, in_builder, "append(request_writer, ");
                                            if (entry.first.size() > 255u)
                                            {
                                                // TODO: avoid this check with a
                                                // better type for the name
                                                throw std::invalid_argument("A method name cannot "
                                                                            "be longer than 255 "
                                                                            "bytes");
                                            }
                                            append(code, boost::lexical_cast<std::string>(
                                                             entry.first.size()));
                                            append(code, "u);\n");
                                            start_line(code, in_builder,
                                                       "append(request_writer, "
                                                       "Si::make_c_str_range(reinterpret_"
                                                       "cast<std::uint8_t const *>(\"");
                                            append(code, entry.first);
                                            append(code, "\")));\n");

                                            std::string error_handling =
                                                "handler(warpcoil::cpp::make_"
                                                "invalid_input_error()); return "
                                                "warpcoil::cpp::client_pipeline_"
                                                "request_status::failure";

                                            for (types::parameter const &param :
                                                 entry.second.parameters)
                                            {
                                                generate_value_serialization(
                                                    code, shared, in_builder,
                                                    Si::make_c_str_range("request_writer"),
                                                    Si::make_contiguous_range(param.name),
                                                    param.type_,
                                                    Si::make_contiguous_range(error_handling));
                                            }
                                            start_line(code, in_builder, "return "
                                                                         "warpcoil::cpp::client_"
                                                                         "pipeline_request_status::"
                                                                         "ok;");
                                        },
                                        "");
                                    append(code, ", handler);\n");
                                    start_line(code, in_method, "return result.get();\n");
                                },
                                "\n\n");
                      }

                      start_line(code, indentation, "private:\n");
                      start_line(code, in_class, "warpcoil::cpp::client_pipeline<"
                                                 "AsyncWriteStream, "
                                                 "AsyncReadStream> "
                                                 "pipeline;\n\n");
                      start_line(code, in_class, "friend std::size_t pending_requests(async_", name,
                                 "_client const &client) { return "
                                 "client.pipeline.pending_requests(); }\n");
                  },
                  ";\n\n");
        }
    }
}
