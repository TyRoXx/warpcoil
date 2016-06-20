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
        void generate_client(CharSink &&code, indentation_level indentation,
                             types::interface_definition const &definition)
        {
            start_line(code, indentation, "function (pending_requests, send_bytes)\n");
            block(code, indentation,
                  [&](indentation_level const in_function)
                  {
                      start_line(code, in_function, "return ");
                      continuation_block(code, in_function,
                                         [&](indentation_level const in_result)
                                         {
                                             for (auto i = definition.methods.begin(); i != definition.methods.end();
                                                  ++i)
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
                                                           Si::ignore_unused_variable_warning(in_method);
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
