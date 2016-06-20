#pragma once

#include <warpcoil/block.hpp>
#include <warpcoil/warpcoil.hpp>
#include <boost/lexical_cast.hpp>

namespace warpcoil
{
    namespace javascript
    {
        size_t find_number_of_bytes_required(std::uint64_t const maximum)
        {
            if (maximum < 0x100)
            {
                return 1;
            }
            if (maximum < 0x10000)
            {
                return 2;
            }
            if (maximum < 0x100000000)
            {
                return 4;
            }
            return 8;
        }

        template <class CharSink>
        void generate_integer_parser(CharSink &&code, indentation_level indentation, std::size_t const number_of_bytes)
        {
            bool const return_64_bit = (number_of_bytes > 4);
            if (return_64_bit)
            {
                Si::append(code, "function (minimum_high, minimum_low, maximum_high, maximum_low)\n");
            }
            else
            {
                Si::append(code, "function (minimum, maximum)\n");
            }
            block(code, indentation,
                  [&](indentation_level const in_function)
                  {
                      if (return_64_bit)
                      {
                          start_line(code, in_function, "var result_high = 0;\n");
                          start_line(code, in_function, "var result_low = 0;\n");
                      }
                      else
                      {
                          start_line(code, in_function, "var result = 0;\n");
                      }
                      start_line(code, in_function, "var parsed = 0;\n");
                      start_line(code, in_function, "return function (input)\n");
                      block(code, in_function,
                            [&](indentation_level const in_result)
                            {
                                if (return_64_bit)
                                {
                                    start_line(code, in_result, "if (parsed > 4)\n");
                                    block(code, in_result,
                                          [&](indentation_level const in_if)
                                          {
                                              start_line(code, in_if, "result_high <<= 8;\n");
                                              start_line(code, in_if, "result_high |= input;\n");
                                          },
                                          "\n");
                                    start_line(code, in_result, "else\n");
                                    block(code, in_result,
                                          [&](indentation_level const in_else)
                                          {
                                              start_line(code, in_else, "result_low <<= 8;\n");
                                              start_line(code, in_else, "result_low |= input;\n");
                                          },
                                          "\n");
                                }
                                else if (number_of_bytes > 1)
                                {
                                    start_line(code, in_result, "result <<= 8;\n");
                                }
                                start_line(code, in_result, "++parsed;\n");
                                start_line(code, in_result, "if (parsed === ",
                                           boost::lexical_cast<std::string>(number_of_bytes), ")\n");
                                block(code, in_result,
                                      [&](indentation_level const in_if)
                                      {
                                          if (return_64_bit)
                                          {
                                              // TODO range check
                                              start_line(code, in_if, "return {high: result_high, low: result_low};\n");
                                          }
                                          else
                                          {
                                              start_line(code, in_if,
                                                         "if ((result < minimum) || (result > maximum))\n");
                                              block(code, in_if,
                                                    [&](indentation_level const in_check)
                                                    {
                                                        start_line(code, in_check, "return undefined;\n");
                                                    },
                                                    "\n");
                                              start_line(code, in_if, "return result;\n");
                                          }
                                      },
                                      "\n");
                                start_line(code, in_result, "return \"more\";\n");
                            },
                            ";\n");
                  },
                  "");
        }

        template <class CharSink>
        void generate_response_parsing(CharSink &&code, indentation_level indentation,
                                       Si::memory_range const common_library)
        {
            start_line(code, indentation, "var request_id_parser = ", common_library,
                       ".parse_int_8(0, 0, 0xffffffff, 0xffffffff);\n");
            start_line(code, indentation, "state = function (input)\n");
            block(code, indentation,
                  [&](indentation_level const in_state)
                  {
                      start_line(code, in_state, "var status = request_id_parser(input);\n");
                      start_line(code, in_state, "if (status === \"more\")\n");
                      block(code, in_state,
                            [&](indentation_level const in_if)
                            {
                                start_line(code, in_if, "return state;\n");
                            },
                            "\n");
                      start_line(code, in_state, "var pending_request = pendings_requests[status];\n");
                      start_line(code, in_state, "if (pending_request === undefined)\n");
                      block(code, in_state,
                            [&](indentation_level const in_if)
                            {
                                start_line(code, in_if, "return undefined;\n");
                            },
                            "\n");
                      start_line(code, in_state, "delete pendings_requests[status];\n");
                      start_line(code, in_state, "return function (input)\n");
                      block(code, in_state,
                            [&](indentation_level const in_state)
                            {
                                start_line(code, in_state, "if (pending_request(input))\n");
                                block(code, in_state,
                                      [&](indentation_level const in_if)
                                      {
                                          start_line(code, in_if, "return initial_state;\n");
                                      },
                                      "\n");
                                start_line(code, in_state, "else\n");
                                block(code, in_state,
                                      [&](indentation_level const in_else)
                                      {
                                          start_line(code, in_else, "return state;\n");
                                      },
                                      "\n");
                            },
                            "\n");
                  },
                  ";\n");
        }

        template <class CharSink>
        void generate_request_parsing(CharSink &&code, indentation_level indentation,
                                      Si::memory_range const common_library)
        {
            start_line(code, indentation, "var request_id_parser = ", common_library,
                       ".parse_int_8(0, 0, 0xffffffff, 0xffffffff);\n");
            start_line(code, indentation, "state = function (input)\n");
            block(code, indentation,
                  [&](indentation_level const in_state)
                  {
                      start_line(code, in_state, "var status = request_id_parser(input);\n");
                      start_line(code, in_state, "if (status === \"more\")\n");
                      block(code, in_state,
                            [&](indentation_level const in_if)
                            {
                                start_line(code, in_if, "return state;\n");
                            },
                            "\n");
                  },
                  ";\n");
        }

        template <class CharSink>
        void generate_input_receiver(CharSink &&code, indentation_level indentation,
                                     Si::memory_range const common_library)
        {
            start_line(code, indentation, "function (pending_requests, server_implementation, send_bytes)\n");
            block(code, indentation,
                  [&](indentation_level const in_function)
                  {
                      start_line(code, in_function, "var state;\n");
                      start_line(code, in_function, "var initial_state = function (input)\n");
                      block(code, in_function,
                            [&](indentation_level const in_first_state)
                            {
                                start_line(code, in_first_state, "if (input === 0)\n");
                                block(code, in_first_state,
                                      [&](indentation_level const in_if)
                                      {
                                          generate_request_parsing(code, in_if, common_library);
                                      },
                                      "\n");
                                start_line(code, in_first_state, "else if (input === 1)\n");
                                block(code, in_first_state,
                                      [&](indentation_level const in_if)
                                      {
                                          generate_response_parsing(code, in_if, common_library);
                                      },
                                      "\n");
                                start_line(code, in_first_state, "else\n");
                                block(code, in_first_state,
                                      [&](indentation_level const in_else)
                                      {
                                          start_line(code, in_else, "return undefined;\n");
                                      },
                                      "\n");
                            },
                            ";\n");
                      start_line(code, in_function, "state = initial_state;\n");
                      start_line(code, in_function, "return ");
                      continuation_block(code, in_function,
                                         [&](indentation_level const in_object)
                                         {
                                             start_line(code, in_object, "parse: function (input)\n");
                                             block(code, in_object,
                                                   [&](indentation_level const in_parse)
                                                   {
                                                       start_line(code, in_parse, "state = state(input);\n");
                                                       start_line(code, in_parse, "return (state !== undefined);\n");
                                                   },
                                                   "\n");
                                         },
                                         ";\n");
                  },
                  "");
        }

        template <class CharSink>
        void generate_size_of_value(CharSink &&code, indentation_level indentation, types::type const &type,
                                    Si::memory_range const value, Si::memory_range const library)
        {
            Si::visit<void>(
                type,
                [&](types::integer const range)
                {
                    Si::append(code, boost::lexical_cast<std::string>(find_number_of_bytes_required(range.maximum)));
                },
                [&](std::unique_ptr<types::variant> const &)
                {
                    throw std::logic_error("todo");
                },
                [&](std::unique_ptr<types::tuple> const &root)
                {
                    Si::append(code, "(0");
                    for (size_t i = 0, c = root->elements.size(); i < c; ++i)
                    {
                        Si::append(code, " + ");
                        std::string element_value;
                        element_value.append(value.begin(), value.size());
                        element_value += "[";
                        element_value += boost::lexical_cast<std::string>(i);
                        element_value += "]";
                        generate_size_of_value(code, indentation, root->elements[i],
                                               Si::make_memory_range(element_value), library);
                    }
                    Si::append(code, ")");
                },
                [&](std::unique_ptr<types::vector> const &root)
                {
                    Si::append(code, "(");
                    Si::append(code,
                               boost::lexical_cast<std::string>(find_number_of_bytes_required(root->length.maximum)));
                    Si::append(code, " + ");
                    Si::append(code, value);
                    Si::append(code, ".reduce(function (previous, next) { return previous + next; }, 0))");
                },
                [&](types::utf8 const text)
                {
                    Si::append(code, "(");
                    Si::append(
                        code, boost::lexical_cast<std::string>(find_number_of_bytes_required(text.code_units.maximum)));
                    Si::append(code, " + ");
                    Si::append(code, library);
                    Si::append(code, ".to_utf8(");
                    Si::append(code, value);
                    Si::append(code, ").length)");
                });
        }

        template <class CharSink>
        void generate_serialization(CharSink &&code, indentation_level indentation, types::type const &type,
                                    Si::memory_range const value, Si::memory_range const destination,
                                    Si::memory_range const offset, Si::memory_range const library)
        {
            Si::visit<void>(type,
                            [&](types::integer const range)
                            {
                                switch (find_number_of_bytes_required(range.maximum))
                                {
                                case 1:
                                    start_line(code, indentation, "new DataView(", destination, ").setUint8(", offset,
                                               ", ", value, ");\n");
                                    break;

                                case 2:
                                    start_line(code, indentation, "new DataView(", destination, ").setUint16(", offset,
                                               ", ", value, ");\n");
                                    break;

                                case 4:
                                    start_line(code, indentation, "new DataView(", destination, ").setUint32(", offset,
                                               ", ", value, ");\n");
                                    break;

                                case 8:
                                    start_line(code, indentation, "new DataView(", destination, ").setUint32(", offset,
                                               ", ", value, ".high);\n");
                                    start_line(code, indentation, "new DataView(", destination, ").setUint32(", offset,
                                               " + 4, ", value, ".low);\n");
                                    break;

                                default:
                                    SILICIUM_UNREACHABLE();
                                }
                            },
                            [&](std::unique_ptr<types::variant> const &)
                            {
                                throw std::logic_error("todo");
                            },
                            [&](std::unique_ptr<types::tuple> const &root)
                            {
                                start_line(code, indentation, "function ()\n");
                                block(code, indentation,
                                      [&](indentation_level const in_function)
                                      {
                                          start_line(code, in_function, "var offset = ", offset, ";\n");
                                          for (size_t i = 0, c = root->elements.size(); i < c; ++i)
                                          {
                                              std::string element_value;
                                              element_value.append(value.begin(), value.size());
                                              element_value += "[";
                                              element_value += boost::lexical_cast<std::string>(i);
                                              element_value += "]";
                                              generate_serialization(code, indentation, root->elements[i],
                                                                     Si::make_memory_range(element_value), destination,
                                                                     Si::make_c_str_range("offset"), library);
                                              start_line(code, in_function, "offset += ");
                                              generate_size_of_value(code, in_function, root->elements[i],
                                                                     Si::make_memory_range(element_value), library);
                                              Si::append(code, ";\n");
                                          }
                                      },
                                      "();\n");
                            },
                            [&](std::unique_ptr<types::vector> const &)
                            {
                                throw std::logic_error("todo");
                            },
                            [&](types::utf8 const text)
                            {
                                std::string length(value.begin(), value.end());
                                length += ".length";
                                generate_serialization(code, indentation, text.code_units,
                                                       Si::make_memory_range(length), destination, offset, library);
                                std::string content_offset = "(";
                                content_offset.append(boost::lexical_cast<std::string>(
                                    find_number_of_bytes_required(text.code_units.maximum)));
                                content_offset.append(" + ");
                                content_offset.append(offset.begin(), offset.end());
                                content_offset.append(")");
                                start_line(code, indentation, "new Uint8Array(", destination, ").set(new Uint8Array(",
                                           library, ".to_utf8(", value, ")), ", content_offset, ");\n");
                            });
        }

        template <class CharSink>
        void generate_client(CharSink &&code, indentation_level indentation,
                             types::interface_definition const &definition, Si::memory_range const library)
        {
            start_line(code, indentation, "function (pending_requests, send_bytes)\n");
            block(code, indentation,
                  [&](indentation_level const in_function)
                  {
                      start_line(code, in_function, "return ");
                      continuation_block(
                          code, in_function,
                          [&](indentation_level const in_result)
                          {
                              for (auto i = definition.methods.begin(); i != definition.methods.end(); ++i)
                              {
                                  auto const &method = *i;
                                  start_line(code, in_result, method.first, ": function (");
                                  for (types::parameter const &parameter : method.second.parameters)
                                  {
                                      Si::append(code, parameter.name);
                                      Si::append(code, ", ");
                                  }
                                  Si::append(code, "on_result)\n");
                                  block(code, in_result,
                                        [&](indentation_level const in_method)
                                        {
                                            start_line(code, in_method, "var request_size = 1 + 8");
                                            for (types::parameter const &parameter : method.second.parameters)
                                            {
                                                Si::append(code, " + ");
                                                generate_size_of_value(code, in_method, parameter.type_,
                                                                       Si::make_memory_range(parameter.name), library);
                                            }
                                            Si::append(code, ";\n");
                                            start_line(code, in_method,
                                                       "var request_buffer = new ArrayBuffer(request_size);\n");
                                            generate_serialization(code, in_method, types::integer{0, 255},
                                                                   Si::make_c_str_range("0"),
                                                                   Si::make_c_str_range("request_buffer"),
                                                                   Si::make_c_str_range("0"), library);
                                            generate_serialization(code, in_method,
                                                                   types::integer{0, 0xffffffffffffffffu},
                                                                   Si::make_c_str_range("{high: 123, low: 456}"),
                                                                   Si::make_c_str_range("request_buffer"),
                                                                   Si::make_c_str_range("1"), library);
                                            if (!method.second.parameters.empty())
                                            {
                                                start_line(code, in_method, "var write_pointer = 1 + 8;\n");
                                            }
                                            for (types::parameter const &parameter : method.second.parameters)
                                            {
                                                generate_serialization(code, in_method, parameter.type_,
                                                                       Si::make_memory_range(parameter.name),
                                                                       Si::make_c_str_range("request_buffer"),
                                                                       Si::make_c_str_range("write_pointer"), library);
                                                start_line(code, in_method, "write_pointer += ");
                                                generate_size_of_value(code, in_method, parameter.type_,
                                                                       Si::make_memory_range(parameter.name), library);
                                                Si::append(code, ";\n");
                                            }
                                        },
                                        "");
                                  if (std::next(i, 1) != definition.methods.end())
                                  {
                                      Si::append(code, ",");
                                  }
                                  Si::append(code, "\n");
                              }
                          },
                          ";\n");
                  },
                  "");
        }

        template <class CharSink>
        void generate_common_library(CharSink &&code, indentation_level indentation)
        {
            Si::append(code, "{\n");
            {
                indentation_level const in_object = indentation.deeper();
                start_line(code, in_object, "to_utf8: function (string)\n");
                {
                    block(code, in_object,
                          [&](indentation_level const in_function)
                          {
                              start_line(code, in_function, "var buffer = new ArrayBuffer(string.length);\n");
                              start_line(code, in_function, "var view = new DataView(buffer);\n");
                              start_line(code, in_function, "for (var i = 0, c = string.length; i < c; ++i)\n");
                              block(code, in_function,
                                    [&](indentation_level const in_for)
                                    {
                                        // TODO: actual UTF-8 encoder
                                        start_line(code, in_for, "view.setUint8(i, string[i]);\n");
                                    },
                                    "\n");
                              start_line(code, in_function, "return buffer;\n");
                          },
                          ",\n");
                }

                start_line(code, in_object, "parse_int_1: ");
                warpcoil::javascript::generate_integer_parser(code, in_object, 1);
                Si::append(code, ",\n");

                start_line(code, in_object, "parse_int_2: ");
                warpcoil::javascript::generate_integer_parser(code, in_object, 2);
                Si::append(code, ",\n");

                start_line(code, in_object, "parse_int_4: ");
                warpcoil::javascript::generate_integer_parser(code, in_object, 4);
                Si::append(code, ",\n");

                start_line(code, in_object, "parse_int_8: ");
                warpcoil::javascript::generate_integer_parser(code, in_object, 8);
                Si::append(code, "\n");
            }
            start_line(code, indentation, "}");
        }
    }
}
