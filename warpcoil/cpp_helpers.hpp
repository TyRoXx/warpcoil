#pragma once

#include <silicium/source/source.hpp>

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
	}
}
