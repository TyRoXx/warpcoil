#pragma once

#include <warpcoil/block.hpp>
#include <warpcoil/cpp/generate/basics.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink1, class CharSink2>
        void generate_value_serialization(CharSink1 &&code, shared_code_generator<CharSink2> &shared,
                                          indentation_level indentation, Si::memory_range sink, Si::memory_range value,
                                          types::type const &type, Si::memory_range return_invalid_input_error)
        {
            return Si::visit<void>(
                type,
                [&](types::integer range)
                {
                    if (range.minimum == 0)
                    {
                        start_line(code, indentation, "if ((", value, ") > ",
                                   boost::lexical_cast<std::string>(range.maximum), "u) { ", return_invalid_input_error,
                                   "; }\n");
                    }
                    else
                    {
                        start_line(code, indentation, "if (((", value, ") < ",
                                   boost::lexical_cast<std::string>(range.minimum), "u) || ((", value, ") > ",
                                   boost::lexical_cast<std::string>(range.maximum), "u)) { ",
                                   return_invalid_input_error, "; }\n");
                    }
                    start_line(code, indentation, "warpcoil::cpp::write_integer(", sink, ", static_cast<",
                               find_suitable_uint_cpp_type(range), ">(", value, "));\n");
                },
                [&](std::unique_ptr<types::variant> const &root)
                {
                    std::string index(value.begin(), value.end());
                    index += ".index()";
                    generate_value_serialization(code, shared, indentation, sink, Si::make_contiguous_range(index),
                                                 types::integer{0, root->elements.size() - 1},
                                                 return_invalid_input_error);

                    start_line(code, indentation, "switch (", value, ".index())\n");
                    block(code, indentation,
                          [&](indentation_level const in_switch)
                          {
                              std::size_t index = 0;
                              for (types::type const &element : root->elements)
                              {
                                  start_line(code, in_switch, "case ", boost::lexical_cast<std::string>(index), ":\n");
                                  indentation_level const in_case = in_switch.deeper();
                                  std::string element_expression = "(*Si::try_get_ptr<";
                                  generate_type(Si::make_container_sink(element_expression), shared, element);
                                  element_expression += ">(";
                                  element_expression.insert(element_expression.end(), value.begin(), value.end());
                                  element_expression += "))";
                                  generate_value_serialization(code, shared, in_case, sink,
                                                               Si::make_contiguous_range(element_expression), element,
                                                               return_invalid_input_error);
                                  start_line(code, in_case, "break;\n");
                                  ++index;
                              }
                          },
                          "\n");
                },
                [&](std::unique_ptr<types::tuple> const &root)
                {
                    std::size_t element_index = 0;
                    for (types::type const &element : root->elements)
                    {
                        std::string element_name = "std::get<" + boost::lexical_cast<std::string>(element_index) + ">(";
                        element_name.insert(element_name.end(), value.begin(), value.end());
                        element_name += ")";
                        generate_value_serialization(code, shared, indentation, sink,
                                                     Si::make_contiguous_range(element_name), element,
                                                     return_invalid_input_error);
                        ++element_index;
                    }
                },
                [&](std::unique_ptr<types::vector> const &root)
                {
                    {
                        std::string size;
                        size.append(value.begin(), value.end());
                        size += ".size()";
                        generate_value_serialization(code, shared, indentation, sink, Si::make_contiguous_range(size),
                                                     root->length, return_invalid_input_error);
                    }
                    indentation.render(code);
                    Si::append(code, "for (auto const &element : ");
                    Si::append(code, value);
                    Si::append(code, ")\n");
                    indentation.render(code);
                    Si::append(code, "{\n");
                    {
                        indentation_level const in_for = indentation.deeper();
                        generate_value_serialization(code, shared, in_for, sink, Si::make_c_str_range("element"),
                                                     root->element, return_invalid_input_error);
                    }
                    indentation.render(code);
                    Si::append(code, "}\n");
                },
                [&](types::utf8 const text)
                {
                    start_line(code, indentation, "if (!utf8::is_valid(", value, ".begin(), ", value, ".end())) { ",
                               return_invalid_input_error, "; }\n");
                    {
                        std::string size;
                        size.append(value.begin(), value.end());
                        size += ".size()";
                        generate_value_serialization(code, shared, indentation, sink, Si::make_contiguous_range(size),
                                                     text.code_units, return_invalid_input_error);
                    }
                    start_line(code, indentation, "Si::append(");
                    Si::append(code, sink);
                    Si::append(code, ", Si::make_iterator_range(reinterpret_cast<std::uint8_t const *>(");
                    Si::append(code, value);
                    Si::append(code, ".data()), reinterpret_cast<std::uint8_t const *>(");
                    Si::append(code, value);
                    Si::append(code, ".data() + ");
                    Si::append(code, value);
                    Si::append(code, ".size())));\n");
                },
                [&](std::unique_ptr<types::structure> const &root)
                {
                    for (types::structure::element const &e : root->elements)
                    {
                        std::string element_name(value.begin(), value.end());
                        element_name += ".";
                        element_name += e.name;
                        generate_value_serialization(code, shared, indentation, sink,
                                                     Si::make_contiguous_range(element_name), e.what,
                                                     return_invalid_input_error);
                    }
                });
        }
    }
}
