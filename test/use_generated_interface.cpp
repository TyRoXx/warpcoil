#include "generated.hpp"
#include <boost/test/unit_test.hpp>

struct add : binary_integer_function
{
	virtual ::std::uint64_t
	evaluate(::std::tuple<::std::uint64_t, ::std::uint64_t> argument) override
	{
		return std::get<0>(argument) + std::get<1>(argument);
	}
};

BOOST_AUTO_TEST_CASE(test_binary_integer_function)
{
	add a;
	BOOST_CHECK_EQUAL(3, a.evaluate(std::make_tuple(1, 2)));
}
