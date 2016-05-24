#pragma once

#include <warpcoil/cpp/parse_result.hpp>
#include <warpcoil/cpp/parser_for.hpp>
#include <silicium/source/source.hpp>
#include <silicium/sink/sink.hpp>
#include <silicium/sink/append.hpp>

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

        template <class T>
        struct parser_for<T, std::enable_if_t<std::is_unsigned<T>::value>>
        {
            typedef integer_parser<T> type;
        };
    }
}
