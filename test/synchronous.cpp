#include "generated.hpp"
#include <boost/test/unit_test.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>

struct add : binary_integer_function
{
	virtual ::std::uint64_t evaluate(::std::tuple<::std::uint64_t, ::std::uint64_t> argument) override
	{
		return std::get<0>(argument) + std::get<1>(argument);
	}
};

BOOST_AUTO_TEST_CASE(test_binary_integer_function)
{
	add a;
	BOOST_CHECK_EQUAL(3, a.evaluate(std::make_tuple(1, 2)));
}

struct impl_test_interface : test_interface
{
	virtual ::std::tuple<> no_result_no_parameter(::std::tuple<> argument) override
	{
		return argument;
	}

	virtual ::std::tuple<::std::uint64_t, ::std::uint64_t> two_results(::std::uint64_t argument) override
	{
		return std::make_tuple(argument, argument + 1);
	}

	virtual ::std::uint64_t two_parameters(::std::tuple<::std::uint64_t, ::std::uint64_t> argument) override
	{
		return std::get<0>(argument) * std::get<1>(argument);
	}

	virtual std::vector<std::uint64_t> vectors(std::vector<std::uint64_t> argument) override
	{
		std::reverse(argument.begin(), argument.end());
		return argument;
	}
};

BOOST_AUTO_TEST_CASE(no_result_no_parameter)
{
	impl_test_interface t;
	BOOST_CHECK(std::make_tuple() == t.no_result_no_parameter(std::make_tuple()));
}

BOOST_AUTO_TEST_CASE(two_results)
{
	impl_test_interface t;
	BOOST_CHECK(std::make_tuple(4, 5) == t.two_results(4));
}

BOOST_AUTO_TEST_CASE(two_parameters)
{
	impl_test_interface t;
	BOOST_CHECK_EQUAL(20, t.two_parameters(std::make_tuple(4, 5)));
}

template <class Argument, class ExpectedResult, class ExpectedRequest, class Response>
void generic_test_serialization_client(ExpectedResult (test_interface::*call)(Argument), Argument argument,
                                       ExpectedResult expected_result, ExpectedRequest expected_request,
                                       Response response)
{
	std::vector<std::uint8_t> request;
	auto request_writer = Si::Sink<std::uint8_t, Si::success>::erase(Si::make_container_sink(request));
	auto response_reader = Si::Source<std::uint8_t>::erase(Si::make_container_source(response));
	test_interface_client t(request_writer, response_reader);
	BOOST_CHECK(expected_result == (t.*call)(argument));
	BOOST_CHECK_EQUAL_COLLECTIONS(expected_request.begin(), expected_request.end(), request.begin(), request.end());
	BOOST_CHECK_EQUAL(Si::none, Si::get(response_reader));
}

BOOST_AUTO_TEST_CASE(serialization_client_no_result_no_parameter)
{
	generic_test_serialization_client(
	    &test_interface::no_result_no_parameter, std::tuple<>(), std::tuple<>(),
	    std::array<std::uint8_t, 1 + 22>{{22, 'n', 'o', '_', 'r', 'e', 's', 'u', 'l', 't', '_', 'n', 'o', '_', 'p', 'a',
	                                      'r', 'a', 'm', 'e', 't', 'e', 'r'}},
	    std::array<std::uint8_t, 0>{{}});
}

BOOST_AUTO_TEST_CASE(serialization_client_two_parameters)
{
	generic_test_serialization_client(
	    &test_interface::two_parameters, std::tuple<std::uint64_t, std::uint64_t>(2, 3), std::uint64_t(6),
	    std::array<std::uint8_t, 1 + 14 + (2 * 8)>{{14, 't', 'w', 'o', '_', 'p', 'a', 'r', 'a', 'm', 'e', 't', 'e', 'r',
	                                                's', 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3}},
	    std::array<std::uint8_t, 8>{{0, 0, 0, 0, 0, 0, 0, 6}});
}

BOOST_AUTO_TEST_CASE(serialization_client_two_results)
{
	generic_test_serialization_client(
	    &test_interface::two_results, std::uint64_t(6), std::tuple<std::uint64_t, std::uint64_t>(6, 7),
	    std::array<std::uint8_t, 1 + 11 + 8>{
	        {11, 't', 'w', 'o', '_', 'r', 'e', 's', 'u', 'l', 't', 's', 0, 0, 0, 0, 0, 0, 0, 6}},
	    std::array<std::uint8_t, 2 * 8>{{0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 7}});
}

BOOST_AUTO_TEST_CASE(serialization_client_vectors)
{
	generic_test_serialization_client(
	    &test_interface::vectors, std::vector<std::uint64_t>{4, 5}, std::vector<std::uint64_t>{5, 4},
	    std::array<std::uint8_t, 1 + 7 + (3 * 8)>{{7, 'v', 'e', 'c', 't', 'o', 'r', 's', 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
	                                               0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 5}},
	    std::array<std::uint8_t, 3 * 8>{{0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 4}});
}

BOOST_AUTO_TEST_CASE(serialization_server)
{
	std::vector<std::uint8_t> response;
	auto response_writer = Si::Sink<std::uint8_t, Si::success>::erase(Si::make_container_sink(response));
	impl_test_interface handler;
	test_interface_server server(handler, response_writer);
	std::array<std::uint8_t, 1 + 14 + (2 * 8)> const request = {{14, 't', 'w', 'o', '_', 'p', 'a', 'r', 'a', 'm', 'e',
	                                                             't', 'e', 'r', 's', 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0,
	                                                             0, 0, 0, 5}};
	Si::append(server, Si::make_contiguous_range(request));
	std::array<std::uint8_t, 8> const expected_response = {{0, 0, 0, 0, 0, 0, 0, 20}};
	BOOST_CHECK_EQUAL_COLLECTIONS(expected_response.begin(), expected_response.end(), response.begin(), response.end());
}
