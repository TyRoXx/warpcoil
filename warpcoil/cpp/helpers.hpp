#pragma once

#include <string>

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
