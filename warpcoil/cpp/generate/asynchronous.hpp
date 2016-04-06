#pragma once

#include <warpcoil/cpp/generate/parser.hpp>

namespace warpcoil
{
	namespace cpp
	{
		namespace async
		{
			template <class CharSink>
			void
			generate_interface(CharSink &&code, indentation_level indentation,
			                   Si::memory_range name,
			                   types::interface_definition const &definition)
			{
				Si::append(code, "struct async_");
				Si::append(code, name);
				Si::append(code, "\n");
				indentation.render(code);
				Si::append(code, "{\n");
				{
					indentation_level const in_class = indentation.deeper();
					in_class.render(code);
					Si::append(code, "virtual ~async_");
					Si::append(code, name);
					Si::append(code, "() {}\n");
					for (auto const &entry : definition.methods)
					{
						in_class.render(code);
						Si::append(code, "virtual void ");
						Si::append(code, entry.first);
						Si::append(code, "(");
						switch (generate_type(code, entry.second.parameter))
						{
						case type_emptiness::empty:
							break;

						case type_emptiness::non_empty:
							Si::append(code, " argument");
							break;
						}
						Si::append(
						    code,
						    ", std::function<void(boost::system::error_code, ");
						generate_type(code, entry.second.result);
						Si::append(code, ")> on_result) = 0;\n");
					}
				}
				indentation.render(code);
				Si::append(code, "};\n\n");
			}

			template <class CharSink>
			void generate_serialization_client(
			    CharSink &&code, indentation_level indentation,
			    Si::memory_range name,
			    types::interface_definition const &definition)
			{
				Si::append(code, "template <class AsyncWriteStream, class "
				                 "AsyncReadStream>\nstruct async_");
				Si::append(code, name);
				Si::append(code, "_client : async_");
				Si::append(code, name);
				Si::append(code, "\n");
				indentation.render(code);
				Si::append(code, "{\n");
				{
					indentation_level const in_class = indentation.deeper();
					in_class.render(code);
					Si::append(code, "explicit async_");
					Si::append(code, name);
					Si::append(code, "_client(AsyncWriteStream &requests, "
					                 "AsyncReadStream "
					                 "&responses)\n");
					in_class.deeper().render(code);
					Si::append(code, ": requests(requests), "
					                 "responses(responses), "
					                 "response_buffer_used(0) {}\n\n");
					for (auto const &entry : definition.methods)
					{
						in_class.render(code);
						Si::append(code, "void ");
						Si::append(code, entry.first);
						Si::append(code, "(");
						switch (generate_type(code, entry.second.parameter))
						{
						case type_emptiness::empty:
							break;

						case type_emptiness::non_empty:
							Si::append(code, " argument");
							break;
						}
						Si::append(
						    code,
						    ", std::function<void(boost::system::error_code, ");
						type_emptiness const result_emptiness =
						    generate_type(code, entry.second.result);
						Si::append(code, ")> on_result) override\n");
						in_class.render(code);
						Si::append(code, "{\n");
						{
							indentation_level const in_method =
							    in_class.deeper();
							in_method.render(code);
							Si::append(code, "auto request_writer = "
							                 "Si::Sink<std::uint8_t, "
							                 "Si::success>::erase(Si::make_"
							                 "container_sink(request_buffer));"
							                 "\n");
							in_method.render(code);
							Si::append(code, "Si::append(request_writer, ");
							if (entry.first.size() > 255u)
							{
								// TODO: avoid this check with a better type for
								// the
								// name
								throw std::invalid_argument(
								    "A method name cannot "
								    "be longer than 255 "
								    "bytes");
							}
							Si::append(code, boost::lexical_cast<std::string>(
							                     entry.first.size()));
							Si::append(code, "u);\n");
							in_method.render(code);
							Si::append(code, "Si::append(request_writer, "
							                 "Si::make_c_str_range(reinterpret_"
							                 "cast<std::uint8_t const *>(\"");
							Si::append(code, entry.first);
							Si::append(code, "\")));\n");
							generate_value_serialization(
							    code, in_method,
							    Si::make_c_str_range("request_writer"),
							    Si::make_c_str_range("argument"),
							    entry.second.parameter);
							in_method.render(code);
							Si::append(code, "boost::asio::async_write("
							                 "requests, "
							                 "boost::asio::buffer(request_"
							                 "buffer), [this, "
							                 "on_result](boost::system::error_"
							                 "code ec, std::size_t)\n");
							in_method.render(code);
							Si::append(code, "{\n");
							{
								indentation_level const in_written =
								    in_method.deeper();
								in_written.render(code);
								switch (result_emptiness)
								{
								case type_emptiness::non_empty:
								{
									Si::append(code,
									           "if (!!ec) { on_result(ec, "
									           "{}); return; }\n");
									in_written.render(code);
									Si::append(code, "struct read_state\n");
									in_written.render(code);
									Si::append(code, "{\n");
									{
										indentation_level const in_struct =
										    in_written.deeper();
										in_struct.render(code);
										Si::append(code, "std::function<void()>"
										                 " begin_parse;\n");
										in_struct.render(code);
										generate_parser_type(
										    code, entry.second.result);
										Si::append(code, " parser;\n");
									}
									in_written.render(code);
									Si::append(code, "};\n");
									in_written.render(code);
									Si::append(code,
									           "auto state = "
									           "std::make_shared<read_state>("
									           ");\n");
									in_written.render(code);
									Si::append(code,
									           "state->begin_parse = [this, "
									           "state, on_result]()\n");
									in_written.render(code);
									Si::append(code, "{\n");
									{
										indentation_level const in_begin =
										    in_written.deeper();
										in_begin.render(code);
										Si::append(
										    code,
										    "for (std::size_t i = 0; i < "
										    "response_buffer_used; ++i)\n");
										in_begin.render(code);
										Si::append(code, "{\n");
										{
											indentation_level const in_loop =
											    in_begin.deeper();
											in_loop.render(code);
											Si::append(code,
											           "if (auto *response = "
											           "state->parser.parse_"
											           "byte(response_buffer["
											           "i]))\n");
											in_loop.render(code);
											Si::append(code, "{\n");
											{
												indentation_level const in_if =
												    in_loop.deeper();
												in_if.render(code);
												Si::append(
												    code,
												    "std::copy(response_"
												    "buffer.begin() + i + 1, "
												    "response_buffer.begin() "
												    "+ response_buffer_used, "
												    "response_buffer.begin())"
												    ";\n");
												in_if.render(code);
												Si::append(code,
												           "response_buffer_"
												           "used -= 1 + "
												           "i;\n");
												in_if.render(code);
												Si::append(code,
												           "on_result(boost::"
												           "system::error_"
												           "code(), "
												           "std::move(*"
												           "response));\n");
												in_if.render(code);
												Si::append(code, "return;\n");
											}
											in_loop.render(code);
											Si::append(code, "}\n");
										}
										in_begin.render(code);
										Si::append(code, "}\n");

										in_begin.render(code);
										Si::append(
										    code,
										    "responses.async_read_some("
										    "boost::asio::buffer(response_"
										    "buffer), [this, state, "
										    "on_result](boost::system::"
										    "error_code ec, std::size_t "
										    "read)\n");
										in_begin.render(code);
										Si::append(code, "{\n");
										{
											indentation_level const in_read =
											    in_begin.deeper();
											in_read.render(code);
											Si::append(code,
											           "if (!!ec) { "
											           "on_result(ec, {}); "
											           "return; }\n");
											in_read.render(code);
											Si::append(code, "response_buffer_"
											                 "used = read;\n");
											in_read.render(code);
											Si::append(
											    code,
											    "state->begin_parse();\n");
										}
										in_begin.render(code);
										Si::append(code, "});\n");
									}
									in_written.render(code);
									Si::append(code, "};\n");
									in_written.render(code);
									Si::append(code, "state->begin_parse();\n");
									break;
								}

								case type_emptiness::empty:
									Si::append(code, "on_result(ec, {});\n");
									break;
								}
							}
							in_method.render(code);
							Si::append(code, "});\n");
						}
						in_class.render(code);
						Si::append(code, "}\n\n");
					}
					indentation.render(code);
					Si::append(code, "private:\n");

					in_class.render(code);
					Si::append(code,
					           "std::vector<std::uint8_t> request_buffer;\n");
					in_class.render(code);
					Si::append(code, "AsyncWriteStream &requests;\n");
					in_class.render(code);
					Si::append(
					    code,
					    "std::array<std::uint8_t, 512> response_buffer;\n");
					in_class.render(code);
					Si::append(code, "std::size_t response_buffer_used;\n");
					in_class.render(code);
					Si::append(code, "AsyncReadStream &responses;\n");
				}
				indentation.render(code);
				Si::append(code, "};\n\n");
			}

			template <class CharSink>
			void generate_serializable_interface(
			    CharSink &&code, indentation_level indentation,
			    Si::memory_range name,
			    types::interface_definition const &definition)
			{
				generate_interface(code, indentation, name, definition);
				generate_serialization_client(code, indentation, name,
				                              definition);
			}
		}
	}
}
