#include "generated.hpp"

struct add : binary_integer_function
{
	virtual ::std::uint64_t evaluate(::std::tuple<::std::uint64_t, ::std::uint64_t> argument) override
	{
		return std::get<0>(argument) + std::get<1>(argument);
	}
};

int main()
{
}
