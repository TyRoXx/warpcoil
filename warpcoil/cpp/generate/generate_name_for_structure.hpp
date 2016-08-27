#pragma once

#include <warpcoil/types.hpp>
#include <silicium/sink/append.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink>
        void generate_name_for_structure(CharSink &&name, types::structure const &structure)
        {
            Si::append(name, "structure_to_do");
            for (types::structure::element const &element : structure.elements)
            {
                Si::append(name, "_");
                Si::append(name, element.name);
            }
        }
    }
}
