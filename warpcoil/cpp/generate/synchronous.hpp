#pragma once

#include <warpcoil/cpp/generate/basics.hpp>

namespace warpcoil
{
	namespace cpp
	{
		template <class CharSink>
		void generate_interface(CharSink &&code, indentation_level indentation,
		                        Si::memory_range name,
		                        types::interface_definition const &definition)
		{
			Si::append(code, "struct ");
			Si::append(code, name);
			Si::append(code, "\n");
			indentation.render(code);
			Si::append(code, "{\n");
			{
				indentation_level const in_class = indentation.deeper();
				in_class.render(code);
				Si::append(code, "virtual ~");
				Si::append(code, name);
				Si::append(code, "() {}\n");
				for (auto const &entry : definition.methods)
				{
					in_class.render(code);
					Si::append(code, "virtual ");
					generate_type(code, entry.second.result);
					Si::append(code, " ");
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
					Si::append(code, ") = 0;\n");
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
			Si::append(code, "struct ");
			Si::append(code, name);
			Si::append(code, "_client : ");
			Si::append(code, name);
			Si::append(code, "\n");
			indentation.render(code);
			Si::append(code, "{\n");
			{
				indentation_level const in_class = indentation.deeper();
				in_class.render(code);
				Si::append(code, "explicit ");
				Si::append(code, name);
				Si::append(code, "_client(Si::Sink<std::uint8_t, "
				                 "Si::success>::interface &requests, "
				                 "Si::Source<std::uint8_t>::interface "
				                 "&responses)\n");
				in_class.deeper().render(code);
				Si::append(code,
				           ": requests(requests), responses(responses) {}\n\n");
				for (auto const &entry : definition.methods)
				{
					in_class.render(code);
					generate_type(code, entry.second.result);
					Si::append(code, " ");
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
					Si::append(code, ") override\n");
					in_class.render(code);
					Si::append(code, "{\n");
					{
						indentation_level const in_method = in_class.deeper();
						in_method.render(code);
						Si::append(code, "Si::append(requests, ");
						if (entry.first.size() > 255u)
						{
							// TODO: avoid this check with a better type for the
							// name
							throw std::invalid_argument("A method name cannot "
							                            "be longer than 255 "
							                            "bytes");
						}
						Si::append(code, boost::lexical_cast<std::string>(
						                     entry.first.size()));
						Si::append(code, "u);\n");
						in_method.render(code);
						Si::append(code, "Si::append(requests, "
						                 "Si::make_c_str_range(reinterpret_"
						                 "cast<std::uint8_t const *>(\"");
						Si::append(code, entry.first);
						Si::append(code, "\")));\n");
						generate_value_serialization(
						    code, in_method, Si::make_c_str_range("requests"),
						    Si::make_c_str_range("argument"),
						    entry.second.parameter);
						in_method.render(code);
						Si::append(code, "return ");
						generate_value_deserialization(
						    code, in_method, Si::make_c_str_range("responses"),
						    entry.second.result);
						Si::append(code, ";\n");
					}
					in_class.render(code);
					Si::append(code, "}\n\n");
				}
				indentation.render(code);
				Si::append(code, "private:\n");

				in_class.render(code);
				Si::append(code, "Si::Sink<std::uint8_t, "
				                 "Si::success>::interface &requests;\n");
				in_class.render(code);
				Si::append(code,
				           "Si::Source<std::uint8_t>::interface &responses;\n");
			}
			indentation.render(code);
			Si::append(code, "};\n\n");
		}

		template <class CharSink>
		void generate_parser_type(CharSink &&code, types::type const &parsed)
		{
			return Si::visit<void>(
			    parsed,
			    [&code](types::integer)
			    {
				    Si::append(code, "warpcoil::cpp::integer_parser");
				},
			    [&code](std::unique_ptr<types::variant> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code](std::unique_ptr<types::tuple> const &parsed)
			    {
				    Si::append(code, "warpcoil::cpp::tuple_parser<");
				    bool first = true;
				    for (types::type const &element : parsed->elements)
				    {
					    if (first)
					    {
						    first = false;
					    }
					    else
					    {
						    Si::append(code, ", ");
					    }
					    generate_parser_type(code, element);
				    }
				    Si::append(code, ">");
				},
			    [&code](std::unique_ptr<types::subset> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code](std::unique_ptr<types::vector> const &parsed)
			    {
				    Si::append(code, "warpcoil::cpp::vector_parser<");
				    generate_parser_type(code, parsed->element);
				    Si::append(code, ">");
				});
		}

		template <class CharSink>
		void generate_serialization_server(
		    CharSink &&code, indentation_level indentation,
		    Si::memory_range name,
		    types::interface_definition const &definition)
		{
			Si::append(code, "struct ");
			Si::append(code, name);
			Si::append(
			    code,
			    "_server : Si::Sink<std::uint8_t, Si::success>::interface");
			Si::append(code, "\n");
			indentation.render(code);
			Si::append(code, "{\n");
			{
				indentation_level const in_class = indentation.deeper();
				in_class.render(code);
				Si::append(code, "explicit ");
				Si::append(code, name);
				Si::append(code, "_server(");
				Si::append(code, name);
				Si::append(code, " &handler, Si::Sink<std::uint8_t, "
				                 "Si::success>::interface &responses)\n");
				in_class.deeper().render(code);
				Si::append(code,
				           ": handler(handler), responses(responses) {}\n\n");

				in_class.render(code);
				Si::append(
				    code, "error_type "
				          "append(Si::iterator_range<element_type const *> data"
				          ") override { for (std::uint8_t element : data) { "
				          "parse_byte(element); } return {}; }\n");

				indentation.render(code);
				Si::append(code, "private:\n");

				for (auto const &entry : definition.methods)
				{
					in_class.render(code);
					Si::append(code, "struct parsing_arguments_of_");
					Si::append(code, entry.first);
					Si::append(code, "\n");
					in_class.render(code);
					Si::append(code, "{\n");
					indentation_level const in_parsing_class =
					    in_class.deeper();
					in_parsing_class.render(code);
					generate_parser_type(code, entry.second.parameter);
					Si::append(code, " parser;\n");
					in_class.render(code);
					Si::append(code, "};\n\n");
				}

				in_class.render(code);
				Si::append(code, name);
				Si::append(code, " &handler;\n");
				in_class.render(code);
				Si::append(code, "Si::Sink<std::uint8_t, "
				                 "Si::success>::interface &responses;\n");
				in_class.render(code);
				Si::append(
				    code,
				    "Si::variant<warpcoil::cpp::parsing_method_name_length, "
				    "warpcoil::cpp::parsing_method_name");
				for (auto const &entry : definition.methods)
				{
					Si::append(code, ", parsing_arguments_of_");
					Si::append(code, entry.first);
				}
				Si::append(code, "> parser_state;\n\n");
				in_class.render(code);
				Si::append(code, "void parse_byte(std::uint8_t const value)\n");
				in_class.render(code);
				Si::append(code, "{\n");
				{
					indentation_level const in_method = in_class.deeper();
					in_method.render(code);
					Si::append(code, "Si::visit<void>(parser_state,\n");
					{
						indentation_level const in_visit = in_method.deeper();
						in_visit.render(code);
						Si::append(code,
						           "  [this, "
						           "value](warpcoil::cpp::parsing_method_name_"
						           "length) "
						           "{ parser_state = "
						           "warpcoil::cpp::parsing_method_name{\"\", "
						           "value}; }\n");
						in_visit.render(code);
						Si::append(code, ", [this, "
						                 "value](warpcoil::cpp::parsing_method_"
						                 "name &parsing)\n");
						in_visit.render(code);
						Si::append(code, "{\n");
						{
							indentation_level const in_lambda =
							    in_visit.deeper();
							in_lambda.render(code);
							Si::append(code,
							           "parsing.name.push_back(value);\n");
							in_lambda.render(code);
							Si::append(code, "if (parsing.name.size() == "
							                 "parsing.expected_length)\n");
							in_lambda.render(code);
							Si::append(code, "{\n");
							{
								indentation_level const block =
								    in_lambda.deeper();
								bool first = true;
								for (auto const &entry : definition.methods)
								{
									block.render(code);
									if (first)
									{
										first = false;
									}
									else
									{
										Si::append(code, "else ");
									}
									Si::append(code, "if (parsing.name == \"");
									Si::append(code, entry.first);
									Si::append(code, "\")\n");
									block.render(code);
									Si::append(code, "{\n");
									{
										indentation_level const in_if =
										    block.deeper();
										in_if.render(code);
										Si::append(code, "parser_state = "
										                 "parsing_arguments_"
										                 "of_");
										Si::append(code, entry.first);
										Si::append(code, "();\n");
									}
									block.render(code);
									Si::append(code, "}\n");
								}
								block.render(code);
								Si::append(code, "else { throw "
								                 "std::logic_error(\"to do: "
								                 "handle unknown method "
								                 "name\"); }\n");
							}
							in_lambda.render(code);
							Si::append(code, "}\n");
						}
						in_visit.render(code);
						Si::append(code, "}\n");
						for (auto const &entry : definition.methods)
						{
							in_visit.render(code);
							Si::append(code,
							           ", [this, value](parsing_arguments_of_");
							Si::append(code, entry.first);
							Si::append(code, " &parsing)\n");
							in_visit.render(code);
							Si::append(code, "{\n");
							{
								indentation_level const in_lambda =
								    in_visit.deeper();
								in_lambda.render(code);
								Si::append(code, "if (auto *argument = "
								                 "parsing.parser.parse_byte("
								                 "value))\n");
								in_lambda.render(code);
								Si::append(code, "{\n");
								{
									indentation_level const in_if =
									    in_lambda.deeper();
									in_if.render(code);
									type_emptiness const result_emptiness =
									    generate_type(code,
									                  entry.second.result);
									Si::append(code, " result = handler.");
									Si::append(code, entry.first);
									Si::append(code,
									           "(std::move(*argument));\n");
									generate_value_serialization(
									    code, in_if,
									    Si::make_c_str_range("responses"),
									    Si::make_c_str_range("result"),
									    entry.second.result);
									switch (result_emptiness)
									{
									case type_emptiness::empty:
										in_if.render(code);
										Si::append(code, "(void)result;\n");
										break;

									case type_emptiness::non_empty:
										break;
									}
									in_if.render(code);
									Si::append(
									    code,
									    "parser_state = "
									    "warpcoil::cpp::parsing_method_name_"
									    "length();\n");
								}
								in_lambda.render(code);
								Si::append(code, "}\n");
							}
							in_visit.render(code);
							Si::append(code, "}\n");
						}
					}
					in_method.render(code);
					Si::append(code, ");\n");
				}
				in_class.render(code);
				Si::append(code, "}\n");
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
			generate_serialization_client(code, indentation, name, definition);
			generate_serialization_server(code, indentation, name, definition);
		}
	}
}
