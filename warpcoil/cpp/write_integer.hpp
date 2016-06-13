#pragma once

#include <silicium/sink/sink.hpp>
#include <silicium/sink/append.hpp>

namespace warpcoil
{
    namespace cpp
    {
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
    }
}
