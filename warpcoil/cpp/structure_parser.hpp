#pragma once

namespace warpcoil
{
    namespace cpp
    {
        template <class Structure>
        struct structure_parser
        {
            typedef Structure result_type;

            parse_result<result_type> parse_byte(std::uint8_t const)
            {
                throw std::logic_error("to do");
            }

            Si::optional<result_type> check_for_immediate_completion() const
            {
                throw std::logic_error("to do");
            }
        };
    }
}
