#pragma once

#include <map>
#include <silicium/variant.hpp>
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
		                    std::unique_ptr<literal>, identifier, std::unique_ptr<tuple_element>>
		    expression;

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
			std::uint64_t minimum;
			std::uint64_t maximum;

			integer()
			    : minimum(0)
			    , maximum((std::numeric_limits<std::uint64_t>::max)())
			{
			}

			integer(std::uint64_t minimum, std::uint64_t maximum)
			    : minimum(minimum)
			    , maximum(maximum)
			{
			}
		};

		struct variant;
		struct tuple;
		struct subset;
		struct vector;

		typedef Si::variant<integer, std::unique_ptr<variant>, std::unique_ptr<tuple>, std::unique_ptr<subset>,
		                    std::unique_ptr<vector>>
		    type;

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

		struct vector
		{
			integer length;
			type element;
		};

		struct interface_definition
		{
			struct method
			{
				type result;
				type parameter;
			};

			std::map<expressions::identifier, method> methods;
		};
	}
}
