#pragma once

#include <boost/lexical_cast.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/sink/ptr_sink.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <warpcoil/types.hpp>
#include <warpcoil/block.hpp>
#include <set>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink>
        struct comma_separator
        {
            explicit comma_separator(CharSink out)
                : m_out(out)
                , m_first(true)
            {
            }

            void add_element()
            {
                if (m_first)
                {
                    m_first = false;
                    return;
                }
                Si::append(m_out, ", ");
            }

        private:
            CharSink m_out;
            bool m_first;
        };

        template <class CharSink>
        auto make_comma_separator(CharSink out)
        {
            return comma_separator<CharSink>(out);
        }

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

        enum class type_emptiness
        {
            empty,
            non_empty
        };

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

        template <class CharSink>
        struct shared_code_generator;

        template <class CharSink1, class CharSink2>
        type_emptiness generate_type(CharSink1 &&code, shared_code_generator<CharSink2> &shared,
                                     types::type const &root);

        template <class CharSink>
        struct shared_code_generator
        {
            explicit shared_code_generator(CharSink code, indentation_level indentation)
                : m_code(std::forward<CharSink>(code))
                , m_indentation(indentation)
            {
            }

            void require_structure(types::structure const &required)
            {
                auto const found = m_generated_structures.find(required);
                if (found != m_generated_structures.end())
                {
                    return;
                }
                start_line(m_code, m_indentation, "struct ");
                generate_name_for_structure(m_code, required);
                Si::append(m_code, "\n");
                block(m_code, m_indentation,
                      [&](indentation_level const in_struct)
                      {
                          for (types::structure::element const &element : required.elements)
                          {
                              start_line(m_code, in_struct, "");
                              generate_type(m_code, *this, element.what);
                              Si::append(m_code, " ");
                              Si::append(m_code, element.name);
                              Si::append(m_code, ";\n");
                          }
                      },
                      ";\n");
                start_line(m_code, m_indentation, "template <> struct warpcoil::cpp::structure_parser<");
                generate_name_for_structure(m_code, required);
                if (required.elements.empty())
                {
                    Si::append(m_code, ">\n");
                    block(m_code, m_indentation,
                          [&](indentation_level const in_struct)
                          {
                              start_line(m_code, in_struct, "typedef ");
                              generate_name_for_structure(m_code, required);
                              Si::append(m_code, " result_type;\n");
                              // TODO
                          },
                          ";\n");
                }
                else
                {
                    Si::append(m_code, "> : warpcoil::cpp::basic_tuple_parser<warpcoil::cpp::structure_parser<");
                    generate_name_for_structure(m_code, required);
                    Si::append(m_code, ">, ");
                    generate_name_for_structure(m_code, required);
                    for (types::structure::element const &element : required.elements)
                    {
                        Si::append(m_code, ", ");
                        generate_type(m_code, *this, element.what);
                    }
                    Si::append(m_code, ">\n ");
                    block(m_code, m_indentation,
                          [&](indentation_level const in_struct)
                          {
                              for (types::structure::element const &element : required.elements)
                              {
                              }
                          },
                          ";\n");
                }
                m_generated_structures.insert(required.clone());
            }

        private:
            struct structure_less
            {
                bool operator()(types::structure const &left, types::structure const &right) const
                {
                    return less(left, right);
                }
            };

            CharSink m_code;
            indentation_level m_indentation;
            std::set<types::structure, structure_less> m_generated_structures;
        };

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
