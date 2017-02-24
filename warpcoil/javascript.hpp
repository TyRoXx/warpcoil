#pragma once

#include <warpcoil/block.hpp>
#include <warpcoil/types.hpp>
#include <warpcoil/comma_separator.hpp>
#include <boost/lexical_cast.hpp>
#include <silicium/sink/ptr_sink.hpp>

namespace warpcoil
{
    namespace javascript
    {
        inline size_t find_number_of_bytes_required(std::uint64_t const maximum)
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
                                    start_line(code, in_result, "if (parsed < 4)\n");
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
                                else
                                {
                                    if (number_of_bytes > 1)
                                    {
                                        start_line(code, in_result, "result <<= 8;\n");
                                    }
                                    start_line(code, in_result, "result |= input;\n");
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
                                                        start_line(
                                                            code, in_check,
                                                            "throw new Error(\"received integer out of range\");\n");
                                                    },
                                                    "\n");
                                              start_line(code, in_if, "return result;\n");
                                          }
                                      },
                                      "\n");
                                start_line(code, in_result, "return undefined;\n");
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
                      start_line(code, in_state, "if (status === undefined)\n");
                      block(code, in_state,
                            [&](indentation_level const in_if)
                            {
                                start_line(code, in_if, "return;\n");
                            },
                            "\n");
                      start_line(code, in_state, "var key = JSON.stringify(status);\n");
                      start_line(code, in_state, "var pending_request = pending_requests[key];\n");
                      start_line(code, in_state, "if (pending_request === undefined)\n");
                      block(code, in_state,
                            [&](indentation_level const in_if)
                            {
                                start_line(code, in_if, "throw new Error(\"Unexpected response: \" + key);\n");
                            },
                            "\n");
                      start_line(code, in_state, "delete pending_requests[key];\n");
                      start_line(code, in_state, "var argument_parser = pending_request.argument_parser;\n");
                      // empty tuple requires no parser, so the field is undefined
                      start_line(code, in_state, "if (argument_parser === undefined)\n");
                      block(code, in_state,
                            [&](indentation_level const in_if)
                            {
                                start_line(code, in_if, "state = initial_state;\n");
                                start_line(code, in_if, "pending_request.on_result(undefined, []);\n");
                                start_line(code, in_if, "return;\n");
                            },
                            "\n");
                      start_line(code, in_state, "state = function (input)\n");
                      block(code, in_state,
                            [&](indentation_level const in_state)
                            {
                                start_line(code, in_state, "var parsed = argument_parser(input);\n");
                                start_line(code, in_state, "if (parsed !== undefined)\n");
                                block(code, in_state,
                                      [&](indentation_level const in_if)
                                      {
                                          start_line(code, in_if, "state = initial_state;\n");
                                          start_line(code, in_if, "pending_request.on_result(undefined, parsed);\n");
                                          start_line(code, in_if, "return;\n");
                                      },
                                      "\n");
                            },
                            "\n");
                  },
                  ";\n");
        }

        template <class CharSink>
        void generate_parser_creation(CharSink &&code, indentation_level const indentation, types::type const &parsed,
                                      Si::memory_range const library)
        {
            Si::visit<void>(
                parsed,
                [&](types::integer const range)
                {
                    Si::append(code, library);
                    switch (find_number_of_bytes_required(range.maximum))
                    {
                    case 1:
                        Si::append(code, ".parse_int_1(");
                        Si::append(code, boost::lexical_cast<std::string>(range.minimum));
                        Si::append(code, ", ");
                        Si::append(code, boost::lexical_cast<std::string>(range.maximum));
                        break;

                    case 2:
                        Si::append(code, ".parse_int_2(");
                        Si::append(code, boost::lexical_cast<std::string>(range.minimum));
                        Si::append(code, ", ");
                        Si::append(code, boost::lexical_cast<std::string>(range.maximum));
                        break;

                    case 4:
                        Si::append(code, ".parse_int_4(");
                        Si::append(code, boost::lexical_cast<std::string>(range.minimum));
                        Si::append(code, ", ");
                        Si::append(code, boost::lexical_cast<std::string>(range.maximum));
                        break;

                    case 8:
                        Si::append(code, ".parse_int_8(");
                        Si::append(code, boost::lexical_cast<std::string>(range.minimum >> 32u));
                        Si::append(code, ", ");
                        Si::append(code, boost::lexical_cast<std::string>(range.minimum & 0xffffffffu));
                        Si::append(code, ", ");
                        Si::append(code, boost::lexical_cast<std::string>(range.maximum >> 32u));
                        Si::append(code, ", ");
                        Si::append(code, boost::lexical_cast<std::string>(range.maximum & 0xffffffffu));
                        break;

                    default:
                        SILICIUM_UNREACHABLE();
                    }
                    Si::append(code, ")");
                },
                [&](std::unique_ptr<types::variant> const &)
                {
                    throw std::logic_error("todo");
                },
                [&](std::unique_ptr<types::tuple> const &root)
                {
                    if (root->elements.empty())
                    {
                        Si::append(code, "undefined");
                    }
                    else
                    {
                        Si::append(code, "function ()\n");
                        block(code, indentation,
                              [&](indentation_level const in_function)
                              {
                                  start_line(code, in_function, "var elements = [];\n");
                                  start_line(code, in_function, "var current_parser = ");
                                  generate_parser_creation(code, in_function, root->elements[0], library);
                                  Si::append(code, ";\n");
                                  start_line(code, in_function, "return function (input)\n");
                                  block(code, in_function,
                                        [&](indentation_level const in_function_2)
                                        {
                                            start_line(code, in_function_2, "switch (elements.length)\n");
                                            block(code, in_function_2,
                                                  [&](indentation_level const in_switch)
                                                  {
                                                      for (size_t i = 0; i < root->elements.size(); ++i)
                                                      {
                                                          start_line(code, in_switch, "case ",
                                                                     boost::lexical_cast<std::string>(i), ":\n");
                                                          block(code, in_switch,
                                                                [&](indentation_level const in_case)
                                                                {
                                                                    start_line(code, in_case,
                                                                               "var status = current_parser(input);\n");
                                                                    start_line(code, in_case,
                                                                               "if (status === undefined)\n");
                                                                    block(code, in_case,
                                                                          [&](indentation_level const in_if)
                                                                          {
                                                                              start_line(code, in_if,
                                                                                         "return undefined;\n");
                                                                          },
                                                                          "\n");
                                                                    start_line(code, in_case,
                                                                               "elements.push(status);\n");
                                                                    if (i == (root->elements.size() - 1))
                                                                    {
                                                                        start_line(code, in_case, "return elements;\n");
                                                                    }
                                                                    else
                                                                    {
                                                                        start_line(code, in_case, "current_parser = ");
                                                                        generate_parser_creation(code, in_case,
                                                                                                 root->elements[i + 1],
                                                                                                 library);
                                                                        Si::append(code, ";\n");
                                                                        start_line(code, in_case, "break;\n");
                                                                    }
                                                                },
                                                                "\n");
                                                      }
                                                  },
                                                  "\n");
                                        },
                                        ";\n");
                              },
                              "()");
                    }
                },
                [&](std::unique_ptr<types::vector> const &)
                {
                    throw std::logic_error("todo");
                },
                [&](types::utf8 const text)
                {
                    Si::append(code, "function ()\n");
                    block(code, indentation,
                          [&](indentation_level const in_function)
                          {
                              start_line(code, in_function, "var length = ");
                              generate_parser_creation(code, in_function, text.code_units, library);
                              Si::append(code, ";\n");
                              start_line(code, in_function, "var content;\n");
                              start_line(code, in_function, "var received_content = 0;\n");
                              start_line(code, in_function, "return function (input)\n");
                              block(code, in_function,
                                    [&](indentation_level const in_function_2)
                                    {
                                        start_line(code, in_function_2, "if (content === undefined)\n");
                                        block(code, in_function_2,
                                              [&](indentation_level const in_if)
                                              {
                                                  start_line(code, in_if, "var status = length(input);\n");
                                                  start_line(code, in_if, "if (status === undefined) { return; }\n");
                                                  start_line(code, in_if, "content = new ArrayBuffer(status);\n");
                                              },
                                              "\n");
                                        start_line(code, in_function_2, "else\n");
                                        block(code, in_function_2,
                                              [&](indentation_level const in_else)
                                              {
                                                  start_line(code, in_else,
                                                             "new Uint8Array(content)[received_content] = input;\n");
                                                  start_line(code, in_else, "++received_content;\n");
                                                  start_line(
                                                      code, in_else,
                                                      "if (received_content !== content.byteLength) { return; }\n");
                                                  start_line(code, in_else, "var result = \"\";\n");
                                                  // TODO: parse UTF-8
                                                  start_line(code, in_else,
                                                             "for (var i = 0; i < content.byteLength; ++i) "
                                                             "{ result += String.fromCharCode(new "
                                                             "Uint8Array(content)[i]); }\n");
                                                  start_line(code, in_else, "return result;\n");
                                              },
                                              "\n");
                                    },
                                    ";\n");
                          },
                          "()");
                },
                [&](std::unique_ptr<types::structure> const &root)
                {
                    if (root->elements.empty())
                    {
                        Si::append(code, "undefined");
                    }
                    else
                    {
                        Si::append(code, "function ()\n");
                        block(code, indentation,
                              [&](indentation_level const in_function)
                              {
                                  start_line(code, in_function, "var result = {};\n");
                                  start_line(code, in_function, "var members_parsed = 0;\n");
                                  start_line(code, in_function, "var current_parser = ");
                                  generate_parser_creation(code, in_function, root->elements[0].what, library);
                                  Si::append(code, ";\n");
                                  start_line(code, in_function, "return function (input)\n");
                                  block(code, in_function,
                                        [&](indentation_level const in_function_2)
                                        {
                                            start_line(code, in_function_2, "switch (members_parsed)\n");
                                            block(code, in_function_2,
                                                  [&](indentation_level const in_switch)
                                                  {
                                                      for (size_t i = 0; i < root->elements.size(); ++i)
                                                      {
                                                          start_line(code, in_switch, "case ",
                                                                     boost::lexical_cast<std::string>(i), ":\n");
                                                          block(code, in_switch,
                                                                [&](indentation_level const in_case)
                                                                {
                                                                    start_line(code, in_case,
                                                                               "var status = current_parser(input);\n");
                                                                    start_line(code, in_case,
                                                                               "if (status === undefined)\n");
                                                                    block(code, in_case,
                                                                          [&](indentation_level const in_if)
                                                                          {
                                                                              start_line(code, in_if,
                                                                                         "return undefined;\n");
                                                                          },
                                                                          "\n");
                                                                    start_line(code, in_case, "result.",
                                                                               root->elements[i].name, " = status;\n");
                                                                    start_line(code, in_case, "++members_parsed;\n");
                                                                    if (i == (root->elements.size() - 1))
                                                                    {
                                                                        start_line(code, in_case, "return result;\n");
                                                                    }
                                                                    else
                                                                    {
                                                                        start_line(code, in_case, "current_parser = ");
                                                                        generate_parser_creation(
                                                                            code, in_case, root->elements[i + 1].what,
                                                                            library);
                                                                        Si::append(code, ";\n");
                                                                        start_line(code, in_case, "break;\n");
                                                                    }
                                                                },
                                                                "\n");
                                                      }
                                                  },
                                                  "\n");
                                        },
                                        ";\n");
                              },
                              "()");
                    }
                });
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
                    Si::append(code, ").byteLength)");
                },
                [&](std::unique_ptr<types::structure> const &root)
                {
                    Si::append(code, "(0");
                    for (types::structure::element const &e : root->elements)
                    {
                        Si::append(code, " + ");
                        std::string element_value;
                        element_value.append(value.begin(), value.size());
                        element_value += ".";
                        element_value += e.name;
                        generate_size_of_value(code, indentation, e.what, Si::make_memory_range(element_value),
                                               library);
                    }
                    Si::append(code, ")");
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
                                if (root->elements.empty())
                                {
                                    return;
                                }
                                start_line(code, indentation, "(function ()\n");
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
                                      ")();\n");
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
                            },
                            [&](std::unique_ptr<types::structure> const &root)
                            {
                                if (root->elements.empty())
                                {
                                    return;
                                }
                                start_line(code, indentation, "(function ()\n");
                                block(code, indentation,
                                      [&](indentation_level const in_function)
                                      {
                                          start_line(code, in_function, "var offset = ", offset, ";\n");
                                          for (types::structure::element const &e : root->elements)
                                          {
                                              std::string element_value;
                                              element_value.append(value.begin(), value.size());
                                              element_value += ".";
                                              element_value += e.name;
                                              generate_serialization(code, indentation, e.what,
                                                                     Si::make_memory_range(element_value), destination,
                                                                     Si::make_c_str_range("offset"), library);
                                              start_line(code, in_function, "offset += ");
                                              generate_size_of_value(code, in_function, e.what,
                                                                     Si::make_memory_range(element_value), library);
                                              Si::append(code, ";\n");
                                          }
                                      },
                                      ")();\n");
                            });
        }

        template <class CharSink>
        void generate_request_parsing(CharSink &&code, indentation_level indentation,
                                      types::interface_definition const &interface,
                                      Si::memory_range const common_library)
        {
            start_line(code, indentation, "var request_id_parser = ", common_library,
                       ".parse_int_8(0, 0, 0xffffffff, 0xffffffff);\n");
            start_line(code, indentation, "state = function (input)\n");
            block(
                code, indentation,
                [&](indentation_level const in_state)
                {
                    start_line(code, in_state, "var status = request_id_parser(input);\n");
                    start_line(code, in_state, "if (status === undefined)\n");
                    block(code, in_state,
                          [&](indentation_level const in_if)
                          {
                              start_line(code, in_if, "return;\n");
                          },
                          "\n");
                    start_line(code, in_state, "var request_id = status;\n");
                    start_line(code, in_state, "var method_name_parser = ");
                    generate_parser_creation(code, in_state, types::utf8{types::integer{0, 255}}, common_library);
                    Si::append(code, ";\n");
                    start_line(code, in_state, "state = function (input)\n");
                    block(
                        code, in_state,
                        [&](indentation_level const in_if)
                        {
                            start_line(code, in_if, "var status = method_name_parser(input);\n");
                            start_line(code, in_if, "if (status === undefined)\n");
                            block(code, in_if,
                                  [&](indentation_level const in_if_2)
                                  {
                                      start_line(code, in_if_2, "return;\n");
                                  },
                                  "\n");
                            for (auto const &method : interface.methods)
                            {
                                start_line(code, in_if, "if (status === \"", method.first, "\")\n");
                                block(
                                    code, in_if,
                                    [&](indentation_level const in_if_2)
                                    {
                                        {
                                            types::tuple parameters;
                                            for (types::parameter const &parameter : method.second.parameters)
                                            {
                                                parameters.elements.emplace_back(clone(parameter.type_));
                                            }
                                            start_line(code, in_if_2, "var parameters = ");
                                            types::type const parameters_erased = Si::to_unique(std::move(parameters));
                                            generate_parser_creation(code, in_if_2, parameters_erased, common_library);
                                            Si::append(code, ";\n");
                                        }
                                        start_line(code, in_if_2, "state = function (input)\n");
                                        block(
                                            code, in_if_2,
                                            [&](indentation_level const in_function)
                                            {
                                                start_line(code, in_function, "var status = parameters(input);\n");
                                                start_line(code, in_function,
                                                           "if (status === undefined) { return undefined; }\n");
                                                start_line(code, in_function, "state = initial_state;\n");
                                                start_line(code, in_function, "server_implementation.", method.first,
                                                           "(");
                                                auto comma = warpcoil::make_comma_separator(Si::ref_sink(code));
                                                for (size_t i = 0; i < method.second.parameters.size(); ++i)
                                                {
                                                    comma.add_element();
                                                    Si::append(code, "status[");
                                                    Si::append(code, boost::lexical_cast<std::string>(i));
                                                    Si::append(code, "]");
                                                }
                                                comma.add_element();
                                                Si::append(code, "function (result)\n");
                                                block(
                                                    code, in_function,
                                                    [&](indentation_level const in_callback)
                                                    {
                                                        start_line(code, in_callback, "var response_size = 1 + 8 + ");
                                                        generate_size_of_value(code, in_callback, method.second.result,
                                                                               Si::make_c_str_range("result"),
                                                                               common_library);
                                                        Si::append(code, ";\n");
                                                        start_line(
                                                            code, in_callback,
                                                            "var response_buffer = new ArrayBuffer(response_size);\n");
                                                        generate_serialization(
                                                            code, in_callback, types::integer{0, 255},
                                                            Si::make_c_str_range("1"),
                                                            Si::make_c_str_range("response_buffer"),
                                                            Si::make_c_str_range("0"), common_library);
                                                        generate_serialization(
                                                            code, in_callback, types::integer{0, 0xffffffffffffffffu},
                                                            Si::make_c_str_range("request_id"),
                                                            Si::make_c_str_range("response_buffer"),
                                                            Si::make_c_str_range("1"), common_library);
                                                        generate_serialization(code, in_callback, method.second.result,
                                                                               Si::make_c_str_range("result"),
                                                                               Si::make_c_str_range("response_buffer"),
                                                                               Si::make_c_str_range("(1 + 8)"),
                                                                               common_library);
                                                        start_line(code, in_callback, "send_bytes(response_buffer);\n");
                                                    },
                                                    "");
                                                Si::append(code, ");\n");
                                            },
                                            ";\n");
                                        start_line(code, in_if_2, "return;\n");
                                    },
                                    "\n");
                            }
                            start_line(code, in_if, "throw new Error(\"unknown method: \" + status);\n");
                        },
                        ";\n");
                },
                ";\n");
        }

        template <class CharSink>
        void generate_input_receiver(CharSink &&code, indentation_level indentation,
                                     types::interface_definition const &interface,
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
                                          generate_request_parsing(code, in_if, interface, common_library);
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
                                          start_line(code, in_else, "throw new Error(\"unknown message type\");\n");
                                      },
                                      "\n");
                            },
                            ";\n");
                      start_line(code, in_function, "state = initial_state;\n");
                      start_line(code, in_function, "return function (input)\n");
                      block(code, in_function,
                            [&](indentation_level const in_parse)
                            {
                                start_line(code, in_parse, common_library, ".assert(undefined === state(input));\n");
                            },
                            ";\n");
                  },
                  "");
        }

        template <class CharSink>
        void generate_client(CharSink &&code, indentation_level indentation,
                             types::interface_definition const &definition, Si::memory_range const library)
        {
            start_line(code, indentation, "function (pending_requests, send_bytes)\n");
            block(code, indentation,
                  [&](indentation_level const in_function)
                  {
                      start_line(code, in_function, "var next_request_id = 0;\n");
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
                                            start_line(code, in_method, "var request_size = 1 + 8 + 1 + ",
                                                       boost::lexical_cast<std::string>(method.first.size()));
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
                                            start_line(code, in_method,
                                                       "var request_id = {high: 0, low: next_request_id};\n");
                                            // TODO: support more than 32 bit request IDs
                                            start_line(code, in_method, "++next_request_id;\n");
                                            generate_serialization(code, in_method,
                                                                   types::integer{0, 0xffffffffffffffffu},
                                                                   Si::make_c_str_range("request_id"),
                                                                   Si::make_c_str_range("request_buffer"),
                                                                   Si::make_c_str_range("1"), library);
                                            start_line(code, in_method, "var write_pointer = 1 + 8;\n");
                                            {
                                                std::string method_name_literal = "\"";
                                                method_name_literal += method.first;
                                                method_name_literal += "\"";
                                                types::type const name_type = types::utf8{types::integer{0, 255}};
                                                generate_serialization(code, in_method, name_type,
                                                                       Si::make_memory_range(method_name_literal),
                                                                       Si::make_c_str_range("request_buffer"),
                                                                       Si::make_c_str_range("write_pointer"), library);
                                                if (!method.second.parameters.empty())
                                                {
                                                    start_line(code, in_method, "write_pointer += ");
                                                    generate_size_of_value(code, in_method, name_type,
                                                                           Si::make_memory_range(method_name_literal),
                                                                           library);
                                                    Si::append(code, ";\n");
                                                }
                                            }
                                            for (size_t k = 0; k < method.second.parameters.size(); ++k)
                                            {
                                                types::parameter const &parameter = method.second.parameters[k];
                                                generate_serialization(code, in_method, parameter.type_,
                                                                       Si::make_memory_range(parameter.name),
                                                                       Si::make_c_str_range("request_buffer"),
                                                                       Si::make_c_str_range("write_pointer"), library);
                                                if (k != (method.second.parameters.size() - 1))
                                                {
                                                    start_line(code, in_method, "write_pointer += ");
                                                    generate_size_of_value(code, in_method, parameter.type_,
                                                                           Si::make_memory_range(parameter.name),
                                                                           library);
                                                    Si::append(code, ";\n");
                                                }
                                            }
                                            start_line(code, in_method, "send_bytes(request_buffer);\n");
                                            start_line(code, in_method, "var key = JSON.stringify(request_id);\n");
                                            start_line(code, in_method, "pending_requests[key] = {argument_parser: ");
                                            generate_parser_creation(code, in_method, method.second.result, library);
                                            Si::append(code, ", on_result: on_result};\n");
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
                                        start_line(code, in_for, "view.setUint8(i, string.charCodeAt(i));\n");
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
                Si::append(code, ",\n");

                start_line(code, in_object, "assert: ");
                Si::append(code, "function (x) { if (!(x)) { throw new Error(\"assertion failed\"); } },\n");

                start_line(code, in_object, "assert_eq: ");
                Si::append(code, "function (expected, got) { if (expected !== got) { throw new Error(\"Expected \" + "
                                 "expected + \", got \" + got); } },\n");

                start_line(code, in_object, "require_type: ");
                Si::append(code, "function (type, value) { assert(typeof(value) === type); }\n");
            }
            start_line(code, indentation, "}");
        }
    }
}
