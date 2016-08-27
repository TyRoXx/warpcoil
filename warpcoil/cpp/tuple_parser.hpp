#pragma once

#include <warpcoil/cpp/parse_result.hpp>
#include <silicium/detail/integer_sequence.hpp>
#include <boost/mpl/at.hpp>
#include <silicium/optional.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class Derived, class Result, class... T>
        struct basic_tuple_parser
        {
            typedef Result result_type;

            parse_result<result_type> parse_byte(std::uint8_t const input)
            {
                switch (Si::apply_visitor(visitor{this, input}, current_element))
                {
                case internal_parse_result::complete_and_consumed:
                    return parse_complete<result_type>{std::move(result), input_consumption::consumed};

                case internal_parse_result::complete_and_not_consumed:
                    return parse_complete<result_type>{std::move(result), input_consumption::does_not_consume};

                case internal_parse_result::incomplete:
                    return need_more_input();

                case internal_parse_result::invalid:
                    return invalid_input();
                }
                SILICIUM_UNREACHABLE();
            }

            Si::optional<result_type> check_for_immediate_completion() const
            {
                std::size_t arity = sizeof...(T);
                if ((arity == 1) && Si::apply_visitor(immediate_completion_visitor(), this->current_element))
                {
                    return std::move(result);
                }
                return Si::none;
            }

        private:
            enum class internal_parse_result
            {
                complete_and_consumed,
                complete_and_not_consumed,
                incomplete,
                invalid
            };

            template <std::size_t I, class Parser>
            struct indexed
            {
                Parser parser;

                internal_parse_result operator()(basic_tuple_parser &parent, std::uint8_t const input)
                {
                    parse_result<typename Parser::result_type> element = parser.parse_byte(input);
                    return Si::visit<internal_parse_result>(
                        element,
                        [&](parse_complete<typename Parser::result_type> &message)
                        {
                            static_cast<Derived &>(parent).get(
                                parent.result, std::integral_constant<std::size_t, I>()) = std::move(message.result);
                            return parent.go_to_next_state(std::integral_constant<std::size_t, I>(), message.input);
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

                bool check_for_immediate_completion() const
                {
                    return !!parser.check_for_immediate_completion();
                }
            };

            struct visitor
            {
                typedef internal_parse_result result_type;

                basic_tuple_parser *parent;
                std::uint8_t input;

                template <class State>
                internal_parse_result operator()(State &state) const
                {
                    return state(*parent, input);
                }
            };

            struct immediate_completion_visitor : boost::static_visitor<bool>
            {
                template <class Indexed>
                bool operator()(Indexed &state) const
                {
                    return state.check_for_immediate_completion();
                }
            };

            template <std::size_t CurrentElement>
            internal_parse_result go_to_next_state(std::integral_constant<std::size_t, CurrentElement>,
                                                   input_consumption const)
            {
                current_element = typename boost::mpl::at<typename decltype(current_element)::element_types,
                                                          boost::mpl::int_<CurrentElement + 1>>::type();
                return internal_parse_result::incomplete;
            }

            internal_parse_result go_to_next_state(std::integral_constant<std::size_t, sizeof...(T)-1>,
                                                   input_consumption const consumption)
            {
                switch (consumption)
                {
                case input_consumption::consumed:
                    return internal_parse_result::complete_and_consumed;

                case input_consumption::does_not_consume:
                    return internal_parse_result::complete_and_not_consumed;
                }
                SILICIUM_UNREACHABLE();
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

        template <class... T>
        struct tuple_parser : basic_tuple_parser<tuple_parser<T...>, std::tuple<typename T::result_type...>, T...>
        {
            template <std::size_t Index>
            auto &get(std::tuple<typename T::result_type...> &result, std::integral_constant<std::size_t, Index>)
            {
                return std::get<Index>(result);
            }
        };

        template <>
        struct tuple_parser<>
        {
            typedef std::tuple<> result_type;

            parse_result<result_type> parse_byte(std::uint8_t const)
            {
                return parse_complete<result_type>{result_type{}, input_consumption::does_not_consume};
            }

            Si::optional<result_type> check_for_immediate_completion() const
            {
                return std::tuple<>();
            }
        };
    }
}
