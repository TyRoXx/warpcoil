#pragma once

#include <warpcoil/cpp/generate/interface.hpp>
#include <warpcoil/cpp/generate/eraser.hpp>
#include <warpcoil/cpp/generate/client.hpp>
#include <warpcoil/cpp/generate/server.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink>
        void generate_serializable_interface(CharSink &&code, indentation_level indentation, Si::memory_range name,
                                             types::interface_definition const &definition)
        {
            generate_interface(code, indentation, name, definition);
            generate_type_eraser(code, indentation, name, definition);
            generate_serialization_client(code, indentation, name, definition);
            generate_serialization_server(code, indentation, name, definition);
        }

        static char const headers[] = "#pragma once\n"
                                      "#include <warpcoil/cpp/begin_parse_value.hpp>\n"
                                      "#include <warpcoil/cpp/client_pipeline.hpp>\n"
                                      "#include <warpcoil/cpp/wrap_handler.hpp>\n"
                                      "#include <warpcoil/cpp/utf8_parser.hpp>\n"
                                      "#include <warpcoil/cpp/integer_parser.hpp>\n"
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
