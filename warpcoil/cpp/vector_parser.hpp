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
                                if (length == 0)
                                {
                                    return std::move(result);
                                }
                                result.reserve(length);
                                assert(result.capacity() == length);
                                step = parsing_element{};
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
                                result.emplace_back(std::move(element));
                                if (result.size() == result.capacity())
                                {
                                    return std::move(result);
                                }
                                step = parsing_element{};
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
            };

            Si::variant<Length, parsing_element> step;
            result_type result;
        };
    }
}
