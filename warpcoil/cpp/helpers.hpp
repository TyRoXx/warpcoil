#pragma once

#include <boost/mpl/at.hpp>
#include <silicium/detail/integer_sequence.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/source/source.hpp>
#include <silicium/variant.hpp>
#include <boost/asio/buffer.hpp>

namespace warpcoil
{
	namespace cpp
	{
		template <class Unsigned>
		Unsigned read_integer(Si::Source<std::uint8_t>::interface &from)
		{
			Unsigned result = 0;
			for (std::size_t i = 0; i < sizeof(result); ++i)
			{
				result = static_cast<std::uint8_t>(result << 8u);
				result = static_cast<std::uint8_t>(result |
				                                   Si::get(from).or_throw([]
				                                                          {
					                                                          throw std::runtime_error(
					                                                              "unexpected end of the response");
					                                                      }));
			}
			return result;
		}

		template <class Unsigned>
		void write_integer(Si::Sink<std::uint8_t>::interface &to, Unsigned const value)
		{
			std::array<std::uint8_t, sizeof(value)> bytes;
			for (std::size_t i = 0; i < sizeof(value); ++i)
			{
				bytes[i] = static_cast<std::uint8_t>(value >> ((sizeof(value) - 1 - i) * 8));
			}
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
				result = static_cast<result_type>(result | input);
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

		template <class Length>
		struct utf8_parser
		{
			typedef std::string result_type;

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
						                                step = parsing_element{0};
					                                }
					                                return nullptr;
					                            },
				                                [this, input](parsing_element &parsing) -> result_type *
				                                {
					                                // TODO: verify UTF-8
					                                result[parsing.current_index] = input;
					                                if (parsing.current_index == (result.size() - 1))
					                                {
						                                return &result;
					                                }
					                                step = parsing_element{parsing.current_index + 1};
					                                return nullptr;
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

		struct parsing_method_name_length
		{
		};

		struct parsing_method_name
		{
			std::string name;
			std::size_t expected_length;
		};

		template <class AsyncReadStream, class Buffer, class Parser, class ResultHandler>
		void begin_parse_value(AsyncReadStream &input, Buffer receive_buffer, std::size_t &receive_buffer_used,
		                       Parser parser, ResultHandler &&on_result)
		{
			for (std::size_t i = 0; i < receive_buffer_used; ++i)
			{
				std::uint8_t *const data = boost::asio::buffer_cast<std::uint8_t *>(receive_buffer);
				if (auto *response = parser.parse_byte(data[i]))
				{
					std::copy(data + i + 1, data + receive_buffer_used, data);
					receive_buffer_used -= 1 + i;
					std::forward<ResultHandler>(on_result)(boost::system::error_code(), std::move(*response));
					return;
				}
			}
			input.async_read_some(receive_buffer,
			                      [
				                    &input,
				                    receive_buffer,
				                    &receive_buffer_used,
				                    parser = std::move(parser),
				                    on_result = std::forward<ResultHandler>(on_result)
				                  ](boost::system::error_code ec, std::size_t read) mutable
			                      {
				                      if (!!ec)
				                      {
					                      std::forward<ResultHandler>(on_result)(ec, {});
					                      return;
				                      }
				                      receive_buffer_used = read;
				                      begin_parse_value(input, receive_buffer, receive_buffer_used, std::move(parser),
				                                        std::forward<ResultHandler>(on_result));
				                  });
		}
	}
}
