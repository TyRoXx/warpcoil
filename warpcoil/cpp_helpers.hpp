#pragma once

#include <silicium/source/source.hpp>
#include <silicium/sink/append.hpp>

namespace warpcoil
{
	namespace cpp
	{
		inline std::uint64_t
		read_integer(Si::Source<std::uint8_t>::interface &from)
		{
			std::uint64_t result = 0;
			for (std::size_t i = 0; i < 8; ++i)
			{
				result <<= 8;
				result |= Si::get(from).or_throw(
				    []
				    {
					    throw std::runtime_error(
					        "unexpected end of the response");
					});
			}
			return result;
		}

		inline void write_integer(Si::Sink<std::uint8_t>::interface &to,
		                          std::uint64_t const value)
		{
			std::array<std::uint8_t, 8> const bytes = {{
			    static_cast<std::uint8_t>(value >> 56),
			    static_cast<std::uint8_t>(value >> 48),
			    static_cast<std::uint8_t>(value >> 40),
			    static_cast<std::uint8_t>(value >> 32),
			    static_cast<std::uint8_t>(value >> 24),
			    static_cast<std::uint8_t>(value >> 16),
			    static_cast<std::uint8_t>(value >> 8),
			    static_cast<std::uint8_t>(value >> 0),
			}};
			Si::append(to, Si::make_contiguous_range(bytes));
		}
	}
}
