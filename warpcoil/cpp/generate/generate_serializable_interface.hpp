#pragma once

#include <warpcoil/cpp/generate/generate_interface.hpp>
#include <warpcoil/cpp/generate/generate_type_eraser.hpp>
#include <warpcoil/cpp/generate/generate_serialization_server.hpp>
#include <warpcoil/cpp/generate/generate_serialization_client.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink1, class CharSink2>
        void generate_serializable_interface(CharSink1 &&code, shared_code_generator<CharSink2> &shared,
                                             indentation_level indentation, Si::memory_range name,
                                             types::interface_definition const &definition)
        {
            generate_interface(code, shared, indentation, name, definition);
            generate_type_eraser(code, shared, indentation, name, definition);
            generate_serialization_client(code, shared, indentation, name, definition);
            generate_serialization_server(code, shared, indentation, name, definition);
        }

        static char const headers[] = "#pragma once\n"
                                      "#include <warpcoil/cpp/begin_parse_value.hpp>\n"
                                      "#include <warpcoil/cpp/client_pipeline.hpp>\n"
                                      "#include <warpcoil/cpp/wrap_handler.hpp>\n"
                                      "#include <warpcoil/cpp/utf8_parser.hpp>\n"
                                      "#include <warpcoil/cpp/integer_parser.hpp>\n"
                                      "#include <warpcoil/cpp/structure_parser.hpp>\n"
                                      "#include <warpcoil/cpp/vector_parser.hpp>\n"
                                      "#include <warpcoil/cpp/tuple_parser.hpp>\n"
                                      "#include <warpcoil/cpp/variant_parser.hpp>\n"
                                      "#include <warpcoil/cpp/write_integer.hpp>\n"
                                      "#include <silicium/source/source.hpp>\n"
                                      "#include <silicium/sink/iterator_sink.hpp>\n"
                                      "#include <silicium/sink/append.hpp>\n"
                                      "#include <silicium/variant.hpp>\n"
                                      "#include <boost/asio/write.hpp>\n"
                                      "#include <boost/range/algorithm/equal.hpp>\n"
                                      "#include <tuple>\n\n";
    }
}
