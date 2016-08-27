#pragma once

#include <warpcoil/cpp/generate/shared_code_generator.hpp>
#include <warpcoil/cpp/generate/comma_separator.hpp>
#include <warpcoil/cpp/generate/generate_name_for_structure.hpp>
#include <silicium/sink/ptr_sink.hpp>

namespace warpcoil
{
    namespace cpp
    {
        inline Si::memory_range find_suitable_uint_cpp_type(types::integer range)
        {
            if (range.maximum <= 0xffu)
            {
                return Si::make_c_str_range("std::uint8_t");
            }
            else if (range.maximum <= 0xffffu)
            {
                return Si::make_c_str_range("std::uint16_t");
            }
            else if (range.maximum <= 0xffffffffu)
            {
                return Si::make_c_str_range("std::uint32_t");
            }
            else
            {
                return Si::make_c_str_range("std::uint64_t");
            }
        }

        template <class CharSink1, class CharSink2>
        type_emptiness generate_type(CharSink1 &&code, shared_code_generator<CharSink2> &shared,
                                     types::type const &root)
        {
            return Si::visit<type_emptiness>(
                root,
                [&code](types::integer range)
                {
                    Si::append(code, find_suitable_uint_cpp_type(range));
                    return type_emptiness::non_empty;
                },
                [&](std::unique_ptr<types::variant> const &root)
                {
                    assert(root);
                    Si::append(code, "Si::variant<");
                    auto comma = make_comma_separator(Si::ref_sink(code));
                    for (types::type const &element : root->elements)
                    {
                        comma.add_element();
                        generate_type(code, shared, element);
                    }
                    Si::append(code, ">");
                    return root->elements.empty() ? type_emptiness::empty : type_emptiness::non_empty;
                },
                [&](std::unique_ptr<types::tuple> const &root)
                {
                    assert(root);
                    Si::append(code, "std::tuple<");
                    auto comma = make_comma_separator(Si::ref_sink(code));
                    for (types::type const &element : root->elements)
                    {
                        comma.add_element();
                        generate_type(code, shared, element);
                    }
                    Si::append(code, ">");
                    return root->elements.empty() ? type_emptiness::empty : type_emptiness::non_empty;
                },
                [&](std::unique_ptr<types::vector> const &root)
                {
                    assert(root);
                    Si::append(code, "std::vector<");
                    generate_type(code, shared, root->element);
                    Si::append(code, ">");
                    return type_emptiness::non_empty;
                },
                [&code](types::utf8)
                {
                    Si::append(code, "std::string");
                    return type_emptiness::non_empty;
                },
                [&](std::unique_ptr<types::structure> const &root) -> type_emptiness
                {
                    assert(root);
                    shared.require_structure(*root);
                    generate_name_for_structure(code, *root);
                    return (root->elements.empty() ? type_emptiness::empty : type_emptiness::non_empty);
                });
        }

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
