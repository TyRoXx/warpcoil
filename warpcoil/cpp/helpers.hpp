#pragma once

#include <boost/mpl/at.hpp>
#include <silicium/detail/integer_sequence.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/source/source.hpp>
#include <silicium/variant.hpp>

namespace warpcoil
{
	namespace cpp
	{
		inline std::uint64_t read_integer(Si::Source<std::uint8_t>::interface &from)
		{
			std::uint64_t result = 0;
			for (std::size_t i = 0; i < 8; ++i)
			{
				result <<= 8;
				result |= Si::get(from).or_throw([]
				                                 {
					                                 throw std::runtime_error("unexpected end of the response");
					                             });
			}
			return result;
		}

		inline void write_integer(Si::Sink<std::uint8_t>::interface &to, std::uint64_t const value)
		{
			std::array<std::uint8_t, 8> const bytes = {{
			    static_cast<std::uint8_t>(value >> 56), static_cast<std::uint8_t>(value >> 48),
			    static_cast<std::uint8_t>(value >> 40), static_cast<std::uint8_t>(value >> 32),
			    static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
			    static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value >> 0),
			}};
			Si::append(to, Si::make_contiguous_range(bytes));
		}

		template <class... T>
		struct tuple_parser
		{
			typedef std::tuple<typename T::result_type...> result_type;

			result_type *parse_byte(std::uint8_t const input)
			{
				switch (Si::apply_visitor(visitor{this, input}, current_element))
				{
				case parse_result::complete:
					return &result;

				case parse_result::incomplete:
					return nullptr;
				}
				SILICIUM_UNREACHABLE();
			}

		private:
			enum class parse_result
			{
				complete,
				incomplete
			};

			template <std::size_t I, class Parser>
			struct indexed
			{
				Parser parser;

				parse_result operator()(tuple_parser &parent, std::uint8_t const input)
				{
					if (auto *parsed = parser.parse_byte(input))
					{
						std::get<I>(parent.result) = std::move(*parsed);
						return parent.go_to_next_state(std::integral_constant<std::size_t, I>());
					}
					return parse_result::incomplete;
				}
			};

			struct visitor
			{
				typedef parse_result result_type;

				tuple_parser *parent;
				std::uint8_t input;

				template <class State>
				parse_result operator()(State &state) const
				{
					return state(*parent, input);
				}
			};

			template <std::size_t CurrentElement>
			parse_result go_to_next_state(std::integral_constant<std::size_t, CurrentElement>)
			{
				current_element = typename boost::mpl::at<typename decltype(current_element)::element_types,
				                                          boost::mpl::int_<CurrentElement + 1>>::type();
				return parse_result::incomplete;
			}

			parse_result go_to_next_state(std::integral_constant<std::size_t, sizeof...(T)-1>)
			{
				return parse_result::complete;
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

			result_type *parse_byte(std::uint8_t const)
			{
				SILICIUM_UNREACHABLE();
			}
		};

		template <class Unsigned>
		struct integer_parser
		{
			typedef Unsigned result_type;

			result_type const *parse_byte(std::uint8_t const input)
			{
				assert(bytes_received < sizeof(result_type));
				result = static_cast<result_type>(result << 8);
				result |= input;
				++bytes_received;
				if (bytes_received == sizeof(result_type))
				{
					return &result;
				}
				return nullptr;
			}

		private:
			result_type result = 0;
			std::size_t bytes_received = 0;
		};

		template <class Length, class Element>
		struct vector_parser
		{
			typedef std::vector<typename Element::result_type> result_type;

			result_type *parse_byte(std::uint8_t const input)
			{
				return Si::visit<result_type *>(step,
				                                [this, input](Length &parser) -> result_type *
				                                {
					                                if (typename Length::result_type const *length =
					                                        parser.parse_byte(input))
					                                {
						                                result.resize(*length);
						                                if (result.empty())
						                                {
							                                return &result;
						                                }
						                                step = parsing_element{{}, 0};
					                                }
					                                return nullptr;
					                            },
				                                [this, input](parsing_element &parsing) -> result_type *
				                                {
					                                if (auto *element = parsing.parser.parse_byte(input))
					                                {
						                                result[parsing.current_index] = std::move(*element);
						                                if (parsing.current_index == (result.size() - 1))
						                                {
							                                return &result;
						                                }
						                                step = parsing_element{{}, parsing.current_index + 1};
					                                }
					                                return nullptr;
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

		struct parsing_method_name_length
		{
		};

		struct parsing_method_name
		{
			std::string name;
			std::size_t expected_length;
		};
	}
}
