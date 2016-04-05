#pragma once

#include <warpcoil/cpp/generate/basics.hpp>

namespace warpcoil
{
	namespace cpp
	{
		template <class CharSink>
		void generate_parser_type(CharSink &&code, types::type const &parsed)
		{
			return Si::visit<void>(
			    parsed,
			    [&code](types::integer)
			    {
				    Si::append(code, "warpcoil::cpp::integer_parser");
				},
			    [&code](std::unique_ptr<types::variant> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code](std::unique_ptr<types::tuple> const &parsed)
			    {
				    Si::append(code, "warpcoil::cpp::tuple_parser<");
				    bool first = true;
				    for (types::type const &element : parsed->elements)
				    {
					    if (first)
					    {
						    first = false;
					    }
					    else
					    {
						    Si::append(code, ", ");
					    }
					    generate_parser_type(code, element);
				    }
				    Si::append(code, ">");
				},
			    [&code](std::unique_ptr<types::subset> const &)
			    {
				    throw std::logic_error("to do");
				},
			    [&code](std::unique_ptr<types::vector> const &parsed)
			    {
				    Si::append(code, "warpcoil::cpp::vector_parser<");
				    generate_parser_type(code, parsed->element);
				    Si::append(code, ">");
				});
		}
	}
}
