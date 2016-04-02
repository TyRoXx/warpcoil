#include <warpcoil/cpp.hpp>
#include <boost/test/unit_test.hpp>
#include <silicium/sink/ptr_sink.hpp>
#include <silicium/sink/ostream_sink.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/sink/append.hpp>
#include <silicium/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

BOOST_AUTO_TEST_CASE(test_generate_function_definition)
{
	using namespace warpcoil;
	types::tuple parameters;
	parameters.elements.emplace_back(types::integer());
	parameters.elements.emplace_back(Si::make_unique<types::tuple>());
	std::string code;
	auto code_writer = Si::make_container_sink(code);
	cpp::generate_function_definition(
	    code_writer, types::integer(), Si::make_c_str_range("func"),
	    Si::to_unique(std::move(parameters)),
	    expressions::closure{
	        expressions::expression{Si::make_unique<expressions::tuple>()}});
	BOOST_CHECK_EQUAL("::std::uint64_t func(::std::tuple<::std::uint64_t, "
	                  "::std::tuple<>> argument)\n"
	                  "{\n"
	                  "    return ::std::make_tuple();\n"
	                  "}\n",
	                  code);
}
