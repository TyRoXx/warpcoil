#pragma once

#include <warpcoil/cpp/generate/interface.hpp>
#include <warpcoil/cpp/generate/eraser.hpp>
#include <warpcoil/cpp/generate/client.hpp>
#include <warpcoil/cpp/generate/server.hpp>

namespace warpcoil
{
    namespace cpp
    {
        namespace async
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
        }
    }
}
