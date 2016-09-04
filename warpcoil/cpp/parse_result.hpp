#pragma once

#include <warpcoil/cpp/invalid_input_error.hpp>
#include <silicium/variant.hpp>

namespace warpcoil
{
    namespace cpp
    {
        enum class input_consumption
        {
            consumed,
            does_not_consume
        };

        template <class T>
        struct parse_complete
        {
            T result;
            input_consumption input;
        };

        struct need_more_input
        {
        };

        template <class T>
        using parse_result = Si::variant<parse_complete<T>, need_more_input, invalid_input>;
    }
}
