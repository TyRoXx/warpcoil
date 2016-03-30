#pragma once

#include <silicium/variant.hpp>
#include <map>
#include <vector>

namespace warpcoil
{
	namespace expressions
	{
		typedef std::string identifier;

		struct call;
		struct tuple;
		struct closure;
		struct literal;
		struct tuple_element;

		typedef Si::variant<std::unique_ptr<call>, std::unique_ptr<tuple>, std::unique_ptr<closure>,
		                    std::unique_ptr<literal>, identifier, std::unique_ptr<tuple_element>> expression;

		struct call
		{
			expression callee;
			expression argument;
		};

		struct tuple
		{
			std::vector<expression> elements;
		};

		struct closure
		{
			expression result;
		};

		struct tuple_element
		{
			expression tuple;
			std::size_t element;
		};
	}

	namespace values
	{
		typedef std::uint64_t integer;

		struct closure;
		struct tuple;

		typedef Si::variant<integer, std::unique_ptr<closure>, std::unique_ptr<tuple>> value;

		struct closure
		{
			std::map<expressions::identifier, value> bound;
			expressions::expression result;
		};

		struct tuple
		{
			std::vector<value> elements;
		};
	}

	namespace expressions
	{
		struct literal
		{
			values::value value;
		};
	}

	namespace types
	{
		struct integer
		{
		};

		struct variant;
		struct tuple;
		struct subset;

		typedef Si::variant<integer, std::unique_ptr<variant>, std::unique_ptr<tuple>, std::unique_ptr<subset>> type;

		struct variant
		{
			std::vector<type> elements;
		};

		struct tuple
		{
			std::vector<type> elements;
		};

		struct subset
		{
			type superset;
			values::closure is_inside;
		};
	}
}
