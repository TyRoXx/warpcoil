#pragma once

#include <warpcoil/cpp/generate/generate_name_for_structure.hpp>
#include <warpcoil/types.hpp>
#include <warpcoil/block.hpp>
#include <boost/lexical_cast.hpp>
#include <set>

namespace warpcoil
{
    namespace cpp
    {
        template <class CharSink>
        void generate_parser_type(CharSink &&code, types::type const &parsed);

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
                warpcoil::cpp::generate_name_for_structure(m_code, required);
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
                start_line(m_code, m_indentation, "namespace warpcoil");
                block(m_code, m_indentation,
                      [&](indentation_level const in_warpcoil)
                      {
                          start_line(m_code, m_indentation, "namespace cpp");
                          block(
                              m_code, in_warpcoil,
                              [&](indentation_level const in_cpp)
                              {
                                  start_line(m_code, in_cpp,
                                             "template <> struct structure_parser<");
                                  warpcoil::cpp::generate_name_for_structure(m_code, required);
                                  if (required.elements.empty())
                                  {
                                      Si::append(m_code, ">\n");
                                      block(m_code, in_cpp,
                                            [&](indentation_level const in_struct)
                                            {
                                                start_line(m_code, in_struct, "typedef ");
                                                warpcoil::cpp::generate_name_for_structure(
                                                    m_code, required);
                                                Si::append(m_code, " result_type;\n");
                                                start_line(m_code, in_struct,
                                                           "parse_result<result_type> "
                                                           "parse_byte(std::uint8_t "
                                                           "const) const "
                                                           "{ return "
                                                           "parse_complete<result_type>{"
                                                           "result_type{}, "
                                                           "input_consumption::does_not_"
                                                           "consume}; }\n");
                                                start_line(m_code, in_struct,
                                                           "Si::optional<result_type> "
                                                           "check_for_immediate_"
                                                           "completion() const { return "
                                                           "result_type{}; }\n");
                                            },
                                            ";\n");
                                  }
                                  else
                                  {
                                      Si::append(m_code, "> : "
                                                         "basic_tuple_parser<"
                                                         "structure_parser<");
                                      warpcoil::cpp::generate_name_for_structure(m_code, required);
                                      Si::append(m_code, ">, ");
                                      warpcoil::cpp::generate_name_for_structure(m_code, required);
                                      for (types::structure::element const &element :
                                           required.elements)
                                      {
                                          Si::append(m_code, ", ");
                                          generate_parser_type(m_code, element.what);
                                      }
                                      Si::append(m_code, ">\n ");
                                      block(m_code, in_cpp,
                                            [&](indentation_level const in_struct)
                                            {
                                                std::size_t index = 0;
                                                for (types::structure::element const &element :
                                                     required.elements)
                                                {
                                                    start_line(m_code, in_struct, "");
                                                    generate_type(m_code, *this, element.what);
                                                    Si::append(m_code, " &get(");
                                                    warpcoil::cpp::generate_name_for_structure(
                                                        m_code, required);
                                                    Si::append(m_code, " &result, "
                                                                       "std::integral_"
                                                                       "constant<std::size_"
                                                                       "t, ");
                                                    Si::append(
                                                        m_code,
                                                        boost::lexical_cast<std::string>(index));
                                                    Si::append(m_code, ">) const\n");
                                                    block(m_code, in_struct,
                                                          [&](indentation_level const in_get)
                                                          {
                                                              start_line(m_code, in_get,
                                                                         "return result.",
                                                                         element.name, ";\n");
                                                          },
                                                          "\n");
                                                    ++index;
                                                }
                                            },
                                            ";\n");
                                  }

                              },
                              "\n");
                      },
                      "\n");
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
    }
}
