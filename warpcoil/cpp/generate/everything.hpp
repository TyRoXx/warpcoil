#pragma once

#include <warpcoil/cpp/generate/asynchronous.hpp>
#include <warpcoil/cpp/generate/synchronous.hpp>

namespace warpcoil
{
	namespace cpp
	{
		template <class CharSink>
		void generate_serializable_interface(CharSink &&code, indentation_level indentation, Si::memory_range name,
		                                     types::interface_definition const &definition)
		{
			sync::generate_serializable_interface(code, indentation, name, definition);
			async::generate_serializable_interface(code, indentation, name, definition);
		}
	}
}
