#pragma once

#include <warpcoil/cpp/generate/parser.hpp>

namespace warpcoil
{
	namespace cpp
	{
		namespace async
		{
			template <class CharSink>
			void generate_interface(CharSink &&code, indentation_level indentation, Si::memory_range name,
			                        types::interface_definition const &definition)
			{
				using Si::append;
				append(code, "struct async_");
				append(code, name);
				append(code, "\n");
				block(code, indentation,
				      [&](indentation_level const in_class)
				      {
					      in_class.render(code);
					      append(code, "virtual ~async_");
					      append(code, name);
					      append(code, "() {}\n\n");
					      for (auto const &entry : definition.methods)
					      {
						      in_class.render(code);
						      append(code, "virtual void ");
						      append(code, entry.first);
						      append(code, "(");
						      generate_parameters(code, entry.second.parameter);
						      append(code, ", std::function<void(boost::system::error_code, ");
						      generate_type(code, entry.second.result);
						      append(code, ")> on_result) = 0;\n");
					      }
					  },
				      ";\n\n");
			}

			template <class CharSink>
			void generate_type_eraser(CharSink &&code, indentation_level indentation, Si::memory_range name,
			                          types::interface_definition const &definition)
			{
				using Si::append;
				start_line(code, indentation, "template <class Client>\n");
				append(code, "struct async_type_erased_");
				append(code, name);
				append(code, " : async_");
				append(code, name);
				append(code, "\n");
				block(code, indentation,
				      [&](indentation_level const in_class)
				      {
					      start_line(code, indentation, "Client &client;\n\n");
					      start_line(code, indentation, "explicit ");
					      append(code, "async_type_erased_");
					      append(code, name);
					      append(code, "(Client &client) : client(client) {}\n\n");
					      start_line(code, indentation, "virtual ~async_type_erased_");
					      append(code, name);
					      append(code, "() {}\n\n");
					      for (auto const &entry : definition.methods)
					      {
						      in_class.render(code);
						      append(code, "virtual void ");
						      append(code, entry.first);
						      append(code, "(");
						      generate_parameters(code, entry.second.parameter);
						      append(code, ", std::function<void(boost::system::error_code, ");
						      type_emptiness const result_emptiness = generate_type(code, entry.second.result);
						      append(code, ")> on_result) override\n");
						      block(code, in_class,
						            [&](indentation_level const in_method)
						            {
							            start_line(code, in_method, "return client.", entry.first, "(");
							            switch (result_emptiness)
							            {
							            case type_emptiness::empty:
								            append(code, "{}");
								            break;

							            case type_emptiness::non_empty:
								            append(code, "std::move(argument)");
								            break;
							            }
							            append(code, ", std::move(on_result));\n");

							        },
						            "\n\n");
					      }
					  },
				      ";\n\n");
			}

			template <class CharSink>
			void generate_serialization_client(CharSink &&code, indentation_level indentation, Si::memory_range name,
			                                   types::interface_definition const &definition)
			{
				using Si::append;
				append(code, "template <class AsyncWriteStream, class "
				             "AsyncReadStream>\nstruct async_");
				append(code, name);
				append(code, "_client");
				append(code, "\n");
				block(code, indentation,
				      [&](indentation_level const in_class)
				      {
					      in_class.render(code);
					      append(code, "explicit async_");
					      append(code, name);
					      append(code, "_client(AsyncWriteStream &requests, "
					                   "AsyncReadStream "
					                   "&responses)\n");
					      in_class.deeper().render(code);
					      append(code, ": requests(requests), "
					                   "responses(responses), "
					                   "response_buffer_used(0) {}\n\n");

					      for (auto const &entry : definition.methods)
					      {
						      in_class.render(code);
						      append(code, "template <class CompletionToken>\n");
						      in_class.render(code);
						      append(code, "auto ");
						      append(code, entry.first);
						      append(code, "(");
						      type_emptiness const parameter_emptiness = generate_type(code, entry.second.parameter);
						      switch (parameter_emptiness)
						      {
						      case type_emptiness::non_empty:
							      append(code, " argument");
							      break;

						      case type_emptiness::empty:
							      break;
						      }
						      append(code, ", CompletionToken &&token)\n");
						      block(code, in_class,
						            [&](indentation_level const in_method)
						            {
							            in_method.render(code);
							            append(code, "using handler_type = typename "
							                         "boost::asio::handler_type<"
							                         "decltype(token), "
							                         "void(boost::system::error_code,"
							                         " ");
							            type_emptiness const result_emptiness =
							                generate_type(code, entry.second.result);
							            append(code, ")>::type;\n");
							            in_method.render(code);
							            append(code, "handler_type "
							                         "handler(std::forward<"
							                         "CompletionToken>(token));\n");
							            in_method.render(code);
							            append(code, "boost::asio::async_result<"
							                         "handler_type> "
							                         "result(handler);\n");
							            in_method.render(code);
							            append(code, "request_buffer.clear();\n");
							            in_method.render(code);
							            append(code, "auto request_writer = "
							                         "Si::Sink<std::uint8_t, "
							                         "Si::success>::erase(Si::make_"
							                         "container_sink(request_buffer));"
							                         "\n");
							            in_method.render(code);
							            append(code, "append(request_writer, ");
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
							            in_method.render(code);
							            append(code, "append(request_writer, "
							                         "Si::make_c_str_range(reinterpret_"
							                         "cast<std::uint8_t const *>(\"");
							            append(code, entry.first);
							            append(code, "\")));\n");
							            generate_value_serialization(
							                code, in_method, Si::make_c_str_range("request_writer"),
							                Si::make_c_str_range("argument"), entry.second.parameter);
							            in_method.render(code);
							            append(code, "boost::asio::async_write("
							                         "requests, "
							                         "boost::asio::buffer(request_"
							                         "buffer), [this, "
							                         "handler](boost::system::error_"
							                         "code ec, std::size_t) mutable\n");
							            block(code, in_method,
							                  [&](indentation_level const in_written)
							                  {
								                  in_written.render(code);
								                  switch (result_emptiness)
								                  {
								                  case type_emptiness::non_empty:
								                  {
									                  append(code, "if (!!ec) { handler(ec, "
									                               "{}); return; }\n");
									                  in_written.render(code);
									                  append(code, "begin_parse_response(responses, "
									                               "boost::asio::buffer(response_buffer), "
									                               "response_buffer_used, ");
									                  generate_parser_type(code, entry.second.result);
									                  append(code, "{}, handler);\n");
									                  break;
								                  }

								                  case type_emptiness::empty:
									                  append(code, "handler(ec, {});\n");
									                  break;
								                  }
								              },
							                  ");\n");
							            in_method.render(code);
							            append(code, "return result.get();\n");
							        },
						            "\n\n");
					      }

					      indentation.render(code);
					      append(code, "private:\n");

					      in_class.render(code);
					      append(code, "std::vector<std::uint8_t> request_buffer;\n");
					      in_class.render(code);
					      append(code, "AsyncWriteStream &requests;\n");
					      in_class.render(code);
					      append(code, "std::array<std::uint8_t, 512> response_buffer;\n");
					      in_class.render(code);
					      append(code, "AsyncReadStream &responses;\n");
					      in_class.render(code);
					      append(code, "std::size_t response_buffer_used;\n");
					  },
				      ";\n\n");
			}

			template <class CharSink>
			void generate_serialization_server(CharSink &&code, indentation_level indentation, Si::memory_range name,
			                                   types::interface_definition const &definition)
			{
				using Si::append;
				append(code, "template <class AsyncReadStream, class AsyncWriteStream>\n");
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
					    append(code, "_server(async_");
					    append(code, name);
					    append(code, " &implementation, AsyncReadStream &requests, AsyncWriteStream "
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
						          append(code, "begin_receive_method_name({}, std::move(handler));\n");
						          in_method.render(code);
						          append(code, "return result.get();\n");
						      },
					          "\n\n");

					    indentation.render(code);
					    append(code, "private:\n");

					    in_class.render(code);
					    append(code, "async_");
					    append(code, name);
					    append(code, " &implementation;\n");
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
					    generate_parser_type(
					        code, Si::to_unique(types::vector{types::integer(0, 255), types::integer(0, 255)}));
					    append(code, " method_name_parser;\n\n");
					    in_class.render(code);
					    append(code, "template <class Handler>\n");
					    in_class.render(code);
					    append(code,
					           "void begin_receive_method_name(method_name_parser name, Handler &&handle_result)\n");
					    block(code, in_class,
					          [&](indentation_level const in_method)
					          {
						          in_method.render(code);
						          append(code, "for (std::size_t i = 0; i < request_buffer_used; ++i)\n");
						          block(code, in_method,
						                [&](indentation_level const in_loop)
						                {
							                in_loop.render(code);
							                append(code, "if (std::vector<std::uint8_t> const *parsed_name = "
							                             "name.parse_byte(request_buffer[i]))\n");
							                block(code, in_loop,
							                      [&](indentation_level const in_if)
							                      {
								                      in_if.render(code);
								                      append(code, "std::copy(request_buffer.begin() + i + 1, "
								                                   "request_buffer.begin() + request_buffer_used, "
								                                   "request_buffer.begin());\n");
								                      in_if.render(code);
								                      append(code, "request_buffer_used -= 1 + i;\n");

								                      bool first = true;
								                      for (auto const &entry : definition.methods)
								                      {
									                      in_if.render(code);
									                      if (first)
									                      {
										                      first = false;
									                      }
									                      else
									                      {
										                      append(code, "else ");
									                      }
									                      append(code, "if (boost::range::equal(*parsed_name, "
									                                   "Si::make_c_str_range(\"");
									                      append(code, entry.first);
									                      append(code, "\")))\n");
									                      block(code, in_if,
									                            [&](indentation_level const in_here)
									                            {
										                            in_here.render(code);
										                            append(code, "begin_receive_method_argument_of_");
										                            append(code, entry.first);
										                            append(code, "({}, std::move(handle_result));\n");
										                        },
									                            "\n");
								                      }
								                      in_if.render(code);
								                      append(code, "else { throw std::logic_error(\"to do: handle "
								                                   "unknown method name\"); }\n");

								                      in_if.render(code);
								                      append(code, "return;\n");
								                  },
							                      "\n");
							            },
						                "\n");

						          in_method.render(code);
						          append(code,
						                 "requests.async_read_some(boost::asio::buffer(request_buffer), [this, "
						                 "handle_result = std::forward<Handler>(handle_result), name = "
						                 "std::move(name)](boost::system::error_code ec, std::size_t read) mutable\n");
						          block(code, in_method,
						                [&](indentation_level const in_read)
						                {
							                in_read.render(code);
							                append(code, "if (!!ec) { handle_result(ec); return; }\n");
							                in_read.render(code);
							                append(code, "request_buffer_used = read;\n");
							                in_read.render(code);
							                append(code, "begin_receive_method_name(std::move(name), "
							                             "std::forward<Handler>(handle_result));\n");
							            },
						                ");\n");
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
						    append(code, "(");
						    generate_parser_type(code, entry.second.parameter);
						    append(code, " argument, Handler &&handle_result)\n");
						    block(
						        code, in_class,
						        [&](indentation_level const in_method)
						        {
							        in_method.render(code);
							        append(code, "for (std::size_t i = 0; i < "
							                     "request_buffer_used; ++i)\n");
							        block(code, in_method,
							              [&](indentation_level const in_loop)
							              {
								              in_loop.render(code);
								              append(code, "if (auto const *parsed_argument = "
								                           "argument.parse_byte(request_buffer[i]))\n");
								              block(code, in_loop,
								                    [&](indentation_level const in_if)
								                    {
									                    in_if.render(code);
									                    append(code, "std::copy(request_buffer"
									                                 ".begin() + i + 1, "
									                                 "request_buffer.begin() "
									                                 "+ request_buffer_used, "
									                                 "request_buffer.begin())"
									                                 ";\n");
									                    in_if.render(code);
									                    append(code, "request_buffer"
									                                 "_used -= 1 + "
									                                 "i;\n");

									                    in_if.render(code);
									                    append(code, "implementation.");
									                    append(code, entry.first);
									                    append(code, "(std::move(*parsed_argument), [this, "
									                                 "handle_result = "
									                                 "std::forward<Handler>(handle_result)](boost::"
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
									                    append(code, ") mutable\n");
									                    block(
									                        code, in_if,
									                        [&](indentation_level const in_lambda)
									                        {
										                        switch (result_empty)
										                        {
										                        case type_emptiness::empty:
											                        in_lambda.render(code);
											                        append(
											                            code,
											                            "std::forward<Handler>(handle_result)(ec);\n");
											                        break;

										                        case type_emptiness::non_empty:
											                        in_lambda.render(code);
											                        append(code,
											                               "if (!!ec) { "
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
											                            Si::make_c_str_range("result"),
											                            entry.second.result);
											                        in_lambda.render(code);
											                        append(code, "boost::asio::async_write(responses,"
											                                     " boost::asio::buffer(response_"
											                                     "buffer), [this, handle_result = "
											                                     "std::move(handle_result)](boost::"
											                                     "system::error_code ec, "
											                                     "std::size_t) mutable\n");
											                        block(code, in_lambda,
											                              [&](indentation_level const in_read)
											                              {
												                              in_read.render(code);
												                              append(code, "std::forward<Handler>("
												                                           "handle_result)(ec);\n");
												                          },
											                              ");\n");
											                        break;
										                        }
										                    },
									                        ");\n");

									                    in_if.render(code);
									                    append(code, "return;\n");
									                },
								                    "\n");
								          },
							              "\n");

							        start_line(
							            code, in_method,
							            "requests.async_read_some(boost::asio::buffer(request_buffer"
							            "), [this, handle_result = std::forward<Handler>(handle_result), argument = "
							            "std::move(argument)](boost::system::error_code ec, std::size_t read) "
							            "mutable\n");
							        block(code, in_method,
							              [&](indentation_level const in_read)
							              {
								              start_line(code, in_read, "if (!!ec) { "
								                                        "std::forward<Handler>(handle_result)(ec); "
								                                        "return; }\n");
								              start_line(code, in_read, "request_buffer_used = read;\n");
								              start_line(code, in_read, "begin_receive_method_argument_of_");
								              append(code, entry.first);
								              append(code,
								                     "(std::move(argument), std::forward<Handler>(handle_result));\n");
								          },
							              ");\n");
							    },
						        "\n");
					    }
					},
				    ";\n\n");
			}

			template <class CharSink>
			void generate_serializable_interface(CharSink &&code, indentation_level indentation, Si::memory_range name,
			                                     types::interface_definition const &definition)
			{
				generate_interface(code, indentation, name, definition);
				generate_type_eraser(code, indentation, name, definition);
				generate_serialization_client(code, indentation, name, definition);
				generate_serialization_server(code, indentation, name, definition);
			}
		}
	}
}
