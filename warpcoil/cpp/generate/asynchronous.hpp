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
				Si::append(code, "struct async_");
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
					Si::append(code, "_client(Si::Sink<std::uint8_t, "
					                 "Si::success>::interface &requests, "
					                 "Si::Source<std::uint8_t>::interface "
					                 "&responses)\n");
					in_class.deeper().render(code);
					Si::append(
					    code,
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
							indentation_level const in_method =
							    in_class.deeper();
							in_method.render(code);
							Si::append(code, "Si::append(requests, ");
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
							Si::append(code, "Si::append(requests, "
							                 "Si::make_c_str_range(reinterpret_"
							                 "cast<std::uint8_t const *>(\"");
							Si::append(code, entry.first);
							Si::append(code, "\")));\n");
							generate_value_serialization(
							    code, in_method,
							    Si::make_c_str_range("requests"),
							    Si::make_c_str_range("argument"),
							    entry.second.parameter);
							in_method.render(code);
							Si::append(code, "return ");
							generate_value_deserialization(
							    code, in_method,
							    Si::make_c_str_range("responses"),
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
					Si::append(
					    code,
					    "Si::Source<std::uint8_t>::interface &responses;\n");
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
