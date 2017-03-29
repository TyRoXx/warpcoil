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
                        parse_result<typename Length::result_type> length =
                            parser.parse_byte(input);
                        return Si::visit<parse_result<result_type>>(
                            length,
                            [this](parse_complete<typename Length::result_type> const length)
                                -> parse_result<result_type>
                            {
                                if (length.result == 0)
                                {
                                    return parse_complete<result_type>{std::move(result),
                                                                       length.input};
                                }
                                result.reserve(length.result);
                                assert(result.capacity() == length.result);
                                step = parsing_element{{}};
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
                        parse_result<typename Element::result_type> element =
                            parsing.parser.parse_byte(input);
                        return Si::visit<parse_result<result_type>>(
                            element,
                            [this, &parsing](parse_complete<typename Element::result_type> element)
                                -> parse_result<result_type>
                            {
                                result.emplace_back(std::move(element.result));
                                if (result.size() == result.capacity())
                                {
                                    return parse_complete<result_type>{std::move(result),
                                                                       element.input};
                                }
                                step = parsing_element{{}};
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

            Si::optional<result_type> check_for_immediate_completion() const
            {
                return Si::none;
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
