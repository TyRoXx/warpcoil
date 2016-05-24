#pragma once

#include <warpcoil/cpp/parse_result.hpp>
#include <silicium/detail/integer_sequence.hpp>
#include <boost/mpl/at.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class... T>
        struct tuple_parser
        {
            typedef std::tuple<typename T::result_type...> result_type;

            parse_result<result_type> parse_byte(std::uint8_t const input)
            {
                switch (Si::apply_visitor(visitor{this, input}, current_element))
                {
                case internal_parse_result::complete:
                    return std::move(result);

                case internal_parse_result::incomplete:
                    return need_more_input();

                case internal_parse_result::invalid:
                    return invalid_input();
                }
                SILICIUM_UNREACHABLE();
            }

        private:
            enum class internal_parse_result
            {
                complete,
                incomplete,
                invalid
            };

            template <std::size_t I, class Parser>
            struct indexed
            {
                Parser parser;

                internal_parse_result operator()(tuple_parser &parent, std::uint8_t const input)
                {
                    parse_result<typename Parser::result_type> element = parser.parse_byte(input);
                    return Si::visit<internal_parse_result>(element,
                                                            [&](typename Parser::result_type &message)
                                                            {
                                                                std::get<I>(parent.result) = std::move(message);
                                                                return parent.go_to_next_state(
                                                                    std::integral_constant<std::size_t, I>());
                                                            },
                                                            [](need_more_input)
                                                            {
                                                                return internal_parse_result::incomplete;
                                                            },
                                                            [](invalid_input)
                                                            {
                                                                return internal_parse_result::invalid;
                                                            });
                }
            };

            struct visitor
            {
                typedef internal_parse_result result_type;

                tuple_parser *parent;
                std::uint8_t input;

                template <class State>
                internal_parse_result operator()(State &state) const
                {
                    return state(*parent, input);
                }
            };

            template <std::size_t CurrentElement>
            internal_parse_result go_to_next_state(std::integral_constant<std::size_t, CurrentElement>)
            {
                current_element = typename boost::mpl::at<typename decltype(current_element)::element_types,
                                                          boost::mpl::int_<CurrentElement + 1>>::type();
                return internal_parse_result::incomplete;
            }

            internal_parse_result go_to_next_state(std::integral_constant<std::size_t, sizeof...(T)-1>)
            {
                return internal_parse_result::complete;
            }

            template <class Integers>
            struct make_current_element;

            template <std::size_t... I>
            struct make_current_element<ranges::v3::integer_sequence<I...>>
            {
                typedef Si::variant<indexed<I, T>...> type;
            };

            typename make_current_element<typename ranges::v3::make_integer_sequence<sizeof...(T)>::type>::type
                current_element;
            result_type result;
        };

        template <>
        struct tuple_parser<>
        {
            typedef std::tuple<> result_type;

            parse_result<result_type> parse_byte(std::uint8_t const)
            {
                SILICIUM_UNREACHABLE();
            }
        };
    }
}
