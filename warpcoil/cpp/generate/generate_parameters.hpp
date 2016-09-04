#pragma once

#include <warpcoil/cpp/generate/shared_code_generator.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink1, class CharSink2>
        type_emptiness generate_parameters(CharSink1 &&code, shared_code_generator<CharSink2> &shared,
                                           std::vector<types::parameter> const &parameters)
        {
            type_emptiness total_emptiness = type_emptiness::empty;
            for (types::parameter const &param : parameters)
            {
                switch (generate_type(code, shared, param.type_))
                {
                case type_emptiness::empty:
                    break;

                case type_emptiness::non_empty:
                    total_emptiness = type_emptiness::non_empty;
                    break;
                }
                Si::append(code, " ");
                Si::append(code, param.name);
                Si::append(code, ", ");
            }
            return total_emptiness;
        }
    }
}
