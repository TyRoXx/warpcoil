#pragma once

#include <warpcoil/cpp/integer_parser.hpp>
#include <warpcoil/cpp/variant_parser.hpp>
#include <warpcoil/cpp/vector_parser.hpp>
#include <warpcoil/cpp/utf8_parser.hpp>
#include <warpcoil/cpp/tuple_parser.hpp>

namespace warpcoil
{
    namespace cpp
    {
        struct parsing_method_name_length
        {
        };

        struct parsing_method_name
        {
            std::string name;
            std::size_t expected_length;
        };
    }
}
