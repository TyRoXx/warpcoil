#pragma once

#include <warpcoil/cpp/parse_result.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class Length, class Element>
        struct vector_parser
        {
            typedef std::vector<typename Element::result_type> result_type;

            parse_result<result_type> parse_byte(std::uint8_t const input)
            {
                return Si::visit<parse_result<result_type>>(
                    step,
                    [this, input](Length &parser) -> parse_result<result_type>
                    {
                        parse_result<typename Length::result_type> length = parser.parse_byte(input);
                        return Si::visit<parse_result<result_type>>(
                            length,
                            [this](typename Length::result_type const length) -> parse_result<result_type>
                            {
                                result.resize(length);
                                if (result.empty())
                                {
                                    return std::move(result);
                                }
                                step = parsing_element{{}, 0};
                                return need_more_input();
                            },
                            [](need_more_input)
                            {
                                return need_more_input();
                            },
                            [](invalid_input)
                            {
                                return invalid_input();
                            });
                    },
                    [this, input](parsing_element &parsing) -> parse_result<result_type>
                    {
                        parse_result<typename Element::result_type> element = parsing.parser.parse_byte(input);
                        return Si::visit<parse_result<result_type>>(
                            element,
                            [this, &parsing](typename Element::result_type element) -> parse_result<result_type>
                            {
                                result[parsing.current_index] = std::move(element);
                                if (parsing.current_index == (result.size() - 1))
                                {
                                    return std::move(result);
                                }
                                step = parsing_element{{}, parsing.current_index + 1};
                                return need_more_input();
                            },
                            [](need_more_input)
                            {
                                return need_more_input();
                            },
                            [](invalid_input)
                            {
                                return invalid_input();
                            });
                    });
            }

        private:
            struct parsing_element
            {
                Element parser;
                std::size_t current_index;
            };

            Si::variant<Length, parsing_element> step;
            result_type result;
        };
    }
}
