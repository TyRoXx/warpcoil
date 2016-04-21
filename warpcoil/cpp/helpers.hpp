#pragma once

#include <boost/mpl/at.hpp>
#include <silicium/detail/integer_sequence.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/source/source.hpp>
#include <silicium/variant.hpp>
#include <boost/asio/buffer.hpp>
#include <utf8/checked.h>

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

		struct need_more_input
		{
		};

		struct invalid_input
		{
		};

		struct invalid_input_error_category : boost::system::error_category
		{
			virtual const char *name() const BOOST_SYSTEM_NOEXCEPT override
			{
				return "invalid_input";
			}

			virtual std::string message(int) const override
			{
				return "invalid input";
			}
		};

		inline boost::system::error_code make_invalid_input_error()
		{
			static invalid_input_error_category const category;
			return boost::system::error_code(1, category);
		}

		template <class T>
		using parse_result = Si::variant<T, need_more_input, invalid_input>;

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

		template <class Unsigned>
		struct integer_parser
		{
			typedef Unsigned result_type;

			parse_result<result_type> parse_byte(std::uint8_t const input)
			{
				assert(bytes_received < sizeof(result_type));
				result = static_cast<result_type>(result << 8);
				result = static_cast<result_type>(result | input);
				++bytes_received;
				if (bytes_received == sizeof(result_type))
				{
					return result;
				}
				return need_more_input();
			}

		private:
			result_type result = 0;
			std::size_t bytes_received = 0;
		};

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
				parse_result<typename Parser::result_type> response = parser.parse_byte(data[i]);
				if (Si::visit<bool>(response,
				                    [&](typename Parser::result_type &message)
				                    {
					                    std::copy(data + i + 1, data + receive_buffer_used, data);
					                    receive_buffer_used -= 1 + i;
					                    std::forward<ResultHandler>(on_result)(boost::system::error_code(),
					                                                           std::move(message));
					                    return true;
					                },
				                    [](need_more_input)
				                    {
					                    return false;
					                },
				                    [&](invalid_input)
				                    {
					                    std::forward<ResultHandler>(on_result)(make_invalid_input_error(), {});
					                    return true;
					                }))
				{
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
