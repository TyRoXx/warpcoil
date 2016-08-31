#pragma once

#include <warpcoil/cpp/generate/basics.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink1, class CharSink2>
        void generate_type_eraser(CharSink1 &&code, shared_code_generator<CharSink2> &shared,
                                  indentation_level indentation, Si::memory_range name,
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
                      for (auto const &entry : definition.methods)
                      {
                          in_class.render(code);
                          append(code, "void ");
                          append(code, entry.first);
                          append(code, "(");
                          generate_parameters(code, shared, entry.second.parameters);
                          append(code, "std::function<void(Si::error_or<");
                          generate_type(code, shared, entry.second.result);
                          append(code, ">)> on_result) override\n");
                          block(code, in_class,
                                [&](indentation_level const in_method)
                                {
                                    start_line(code, in_method, "return client.", entry.first, "(");
                                    for (types::parameter const &parameter : entry.second.parameters)
                                    {
                                        append(code, "std::move(");
                                        append(code, parameter.name);
                                        append(code, "), ");
                                    }
                                    append(code, "std::move(on_result));\n");

                                },
                                "\n\n");
                      }
                  },
                  ";\n\n");
        }
    }
}
