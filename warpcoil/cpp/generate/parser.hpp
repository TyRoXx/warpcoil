#pragma once

#include <warpcoil/cpp/generate/basics.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink>
        void generate_parser_type(CharSink &&code, types::type const &parsed)
        {
            return Si::visit<void>(parsed,
                                   [&](types::integer const parsed)
                                   {
                                       Si::memory_range bits;
                                       if (parsed.maximum <= 0xffu)
                                       {
                                           bits = Si::make_c_str_range("8");
                                       }
                                       else if (parsed.maximum <= 0xffffu)
                                       {
                                           bits = Si::make_c_str_range("16");
                                       }
                                       else if (parsed.maximum <= 0xffffffffu)
                                       {
                                           bits = Si::make_c_str_range("32");
                                       }
                                       else
                                       {
                                           bits = Si::make_c_str_range("64");
                                       }
                                       Si::append(code, "warpcoil::cpp::integer_parser<std::uint");
                                       Si::append(code, bits);
                                       Si::append(code, "_t>");
                                   },
                                   [&](std::unique_ptr<types::variant> const &parsed)
                                   {
                                       Si::append(code, "warpcoil::cpp::variant_parser<");
                                       bool first = true;
                                       for (types::type const &element : parsed->elements)
                                       {
                                           if (first)
                                           {
                                               first = false;
                                           }
                                           else
                                           {
                                               Si::append(code, ", ");
                                           }
                                           generate_parser_type(code, element);
                                       }
                                       Si::append(code, ">");
                                   },
                                   [&](std::unique_ptr<types::tuple> const &parsed)
                                   {
                                       Si::append(code, "warpcoil::cpp::tuple_parser<");
                                       bool first = true;
                                       for (types::type const &element : parsed->elements)
                                       {
                                           if (first)
                                           {
                                               first = false;
                                           }
                                           else
                                           {
                                               Si::append(code, ", ");
                                           }
                                           generate_parser_type(code, element);
                                       }
                                       Si::append(code, ">");
                                   },
                                   [&](std::unique_ptr<types::vector> const &parsed)
                                   {
                                       Si::append(code, "warpcoil::cpp::vector_parser<");
                                       generate_parser_type(code, parsed->length);
                                       Si::append(code, ", ");
                                       generate_parser_type(code, parsed->element);
                                       Si::append(code, ">");
                                   },
                                   [&](types::utf8 const text)
                                   {
                                       Si::append(code, "warpcoil::cpp::utf8_parser<");
                                       generate_parser_type(code, text.code_units);
                                       Si::append(code, ">");
                                   },
                                   [](std::unique_ptr<types::structure> const &)
                                   {
                                       throw std::logic_error("not implemented");
                                   });
        }
    }
}
