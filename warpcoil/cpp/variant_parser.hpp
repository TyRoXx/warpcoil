#pragma once

#include <warpcoil/cpp/parse_result.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class... ElementParsers>
        struct variant_parser
        {
            typedef Si::variant<typename ElementParsers::result_type...> result_type;

            parse_result<result_type> parse_byte(std::uint8_t const input)
            {
                return Si::apply_visitor(element_parser_visitor(*this, input), m_state);
            }

        private:
            typedef std::uint8_t index_type;

            struct parsing_index
            {
                integer_parser<index_type> parser;
            };

            struct element_parser_visitor : boost::static_visitor<parse_result<result_type>>
            {
                typedef typename variant_parser::result_type parent_result_type;

                variant_parser &parent;
                std::uint8_t const input;

                explicit element_parser_visitor(variant_parser &parent, std::uint8_t const input)
                    : parent(parent)
                    , input(input)
                {
                }

                template <class Parser>
                static void switch_to_state(variant_parser &parent)
                {
                    parent.m_state = Parser();
                }

                parse_result<parent_result_type> operator()(parsing_index &state) const
                {
                    return Si::visit<parse_result<parent_result_type>>(
                        state.parser.parse_byte(input),
                        [this](parse_complete<index_type> const index) -> parse_result<parent_result_type>
                        {
                            static std::array<void (*)(variant_parser &), sizeof...(ElementParsers)> const transitions =
                                {{&switch_to_state<ElementParsers>...}};
                            if (index.result >= transitions.size())
                            {
                                return invalid_input();
                            }
                            transitions[index.result](this->parent);
                            return need_more_input();
                        },
                        [](need_more_input error)
                        {
                            return error;
                        },
                        [](invalid_input error)
                        {
                            return error;
                        });
                }

                template <class Parser>
                parse_result<parent_result_type> operator()(Parser &state) const
                {
                    return Si::visit<parse_result<parent_result_type>>(
                        state.parse_byte(input),
                        [](parse_complete<typename Parser::result_type> parsed) -> parse_result<parent_result_type>
                        {
                            return parse_complete<parent_result_type>{std::move(parsed.result), parsed.input};
                        },
                        [](need_more_input input)
                        {
                            return input;
                        },
                        [](invalid_input input)
                        {
                            return input;
                        });
                }
            };

            Si::variant<parsing_index, ElementParsers...> m_state;
        };
    }
}
