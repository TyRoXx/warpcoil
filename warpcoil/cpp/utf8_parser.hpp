#pragma once

#include <warpcoil/cpp/parse_result.hpp>
#include <utf8/checked.h>

namespace warpcoil
{
    namespace cpp
    {
        template <class Length>
        struct utf8_parser
        {
            typedef std::string result_type;

            parse_result<result_type> parse_byte(std::uint8_t const input)
            {
                return Si::visit<parse_result<result_type>>(
                    step,
                    [this, input](Length &parser) -> parse_result<result_type>
                    {
                        parse_result<typename Length::result_type> const length = parser.parse_byte(input);
                        return Si::visit<parse_result<result_type>>(
                            length,
                            [this](typename Length::result_type const length) -> parse_result<result_type>
                            {
                                result.resize(length);
                                if (result.empty())
                                {
                                    return std::move(result);
                                }
                                step = parsing_element{0};
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
                        result[parsing.current_index] = input;
                        if (parsing.current_index == (result.size() - 1))
                        {
                            if (utf8::is_valid(result.begin(), result.end()))
                            {
                                return std::move(result);
                            }
                            else
                            {
                                return invalid_input();
                            }
                        }
                        step = parsing_element{parsing.current_index + 1};
                        return need_more_input();
                    });
            }

        private:
            struct parsing_element
            {
                std::size_t current_index;
            };

            Si::variant<Length, parsing_element> step;
            result_type result;
        };
    }
}
