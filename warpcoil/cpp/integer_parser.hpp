#pragma once

#include <warpcoil/cpp/parse_result.hpp>

namespace warpcoil
{
    namespace cpp
    {
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
                    return parse_complete<result_type>{result, input_consumption::consumed};
                }
                return need_more_input();
            }

            Si::optional<result_type> check_for_immediate_completion() const
            {
                return Si::none;
            }

        private:
            result_type result = 0;
            std::size_t bytes_received = 0;
        };
    }
}
