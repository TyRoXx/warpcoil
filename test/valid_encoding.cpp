#include "test_streams.hpp"
#include "checkpoint.hpp"
#include "generated.hpp"
#include "impl_test_interface.hpp"
#include "boost_print_log_value.hpp"
#include <silicium/exchange.hpp>
#include <silicium/error_or.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#include <boost/test/data/test_case.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace
{
    template <class Result>
    Result test_simple_request_response(
        std::function<void(async_test_interface &, std::function<void(boost::system::error_code, Result)>)>
            begin_request,
        std::vector<std::uint8_t> expected_request, std::vector<std::uint8_t> expected_response)
    {
        warpcoil::impl_test_interface server_impl;
        warpcoil::async_read_stream server_requests;
        warpcoil::async_write_stream server_responses;
        warpcoil::cpp::message_splitter<decltype(server_requests)> server_splitter(server_requests);
        async_test_interface_server<decltype(server_impl), warpcoil::async_read_stream, warpcoil::async_write_stream>
            server(server_impl, server_splitter, server_responses);
        BOOST_REQUIRE(!server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);
        warpcoil::checkpoint request_served;
        server.serve_one_request([&request_served](boost::system::error_code ec)
                                 {
                                     request_served.enter();
                                     BOOST_REQUIRE(!ec);
                                 });
        BOOST_REQUIRE(server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);

        warpcoil::async_write_stream client_requests;
        warpcoil::async_read_stream client_responses;
        warpcoil::cpp::message_splitter<decltype(client_responses)> client_splitter(client_responses);
        warpcoil::cpp::buffered_writer<decltype(client_requests)> writer(client_requests);
        writer.async_run([](boost::system::error_code const)
                         {
                             BOOST_FAIL("Unexpected error");
                         });
        async_test_interface_client<warpcoil::async_write_stream, warpcoil::async_read_stream> client(writer,
                                                                                                      client_splitter);
        BOOST_REQUIRE(!client_responses.respond);
        BOOST_REQUIRE(!client_requests.handle_result);

        warpcoil::checkpoint response_received;
        Result returned_result;
        async_type_erased_test_interface<decltype(client)> type_erased_client{client};
        begin_request(type_erased_client,
                      [&response_received, &returned_result](boost::system::error_code ec, Result result)
                      {
                          response_received.enter();
                          BOOST_REQUIRE(!ec);
                          returned_result = std::move(result);
                      });

        BOOST_REQUIRE(server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);
        BOOST_REQUIRE(server_responses.written.empty());
        BOOST_REQUIRE(client_responses.respond);
        BOOST_REQUIRE(client_requests.handle_result);
        std::vector<std::vector<std::uint8_t>> const expected_request_buffers = {expected_request};
        BOOST_REQUIRE_EQUAL_COLLECTIONS(expected_request_buffers.begin(), expected_request_buffers.end(),
                                        client_requests.written.begin(), client_requests.written.end());
        Si::exchange(server_requests.respond, nullptr)(Si::make_memory_range(client_requests.written[0]));
        client_requests.written.clear();

        Si::exchange(client_requests.handle_result, nullptr)({}, expected_request.size());
        BOOST_REQUIRE(!client_requests.handle_result);

        BOOST_REQUIRE(!server_requests.respond);
        BOOST_REQUIRE(server_responses.handle_result);
        std::vector<std::vector<std::uint8_t>> const expected_response_buffers = {expected_response};
        BOOST_REQUIRE_EQUAL_COLLECTIONS(expected_response_buffers.begin(), expected_response_buffers.end(),
                                        server_responses.written.begin(), server_responses.written.end());
        request_served.enable();
        Si::exchange(server_responses.handle_result, nullptr)({}, expected_response.size());
        request_served.require_crossed();

        BOOST_REQUIRE(!server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);
        BOOST_REQUIRE(client_responses.respond);
        BOOST_REQUIRE(!client_requests.handle_result);

        response_received.enable();
        Si::exchange(client_responses.respond, nullptr)(Si::make_memory_range(expected_response));
        response_received.require_crossed();

        BOOST_REQUIRE(!server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);
        BOOST_REQUIRE(!client_responses.respond);
        BOOST_REQUIRE(!client_requests.handle_result);
        return returned_result;
    }
}

BOOST_AUTO_TEST_CASE(async_server_utf8)
{
    std::string result = test_simple_request_response<std::string>(
        [](async_test_interface &client, std::function<void(boost::system::error_code, std::string)> on_result)
        {
            client.utf8("Name", on_result);
        },
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'u', 't', 'f', '8', 4, 'N', 'a', 'm', 'e'},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 7, 'N', 'a', 'm', 'e', '1', '2', '3'});
    BOOST_CHECK_EQUAL("Name123", result);
}

BOOST_AUTO_TEST_CASE(async_server_vector)
{
    std::vector<std::uint64_t> result = test_simple_request_response<std::vector<std::uint64_t>>(
        [](async_test_interface &client,
           std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result)
        {
            client.vectors(std::vector<std::uint64_t>{3, 2, 1}, on_result);
        },
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 'v', 'e', 'c', 't', 'o', 'r', 's', 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
         3, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
         0, 0, 0, 3});
    std::vector<std::uint64_t> const expected = {1, 2, 3};
    BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), result.begin(), result.end());
}

BOOST_AUTO_TEST_CASE(async_server_tuple)
{
    std::tuple<std::uint64_t, std::uint64_t> result =
        test_simple_request_response<std::tuple<std::uint64_t, std::uint64_t>>(
            [](async_test_interface &client,
               std::function<void(boost::system::error_code, std::tuple<std::uint64_t, std::uint64_t>)> on_result)
            {
                client.two_results(123, on_result);
            },
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 't', 'w', 'o', '_', 'r', 'e', 's', 'u', 'l', 't', 's', 0, 0, 0, 0, 0, 0, 0,
             123},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 123, 0, 0, 0, 0, 0, 0, 0, 123});
    std::tuple<std::uint64_t, std::uint64_t> const expected{123, 123};
    BOOST_CHECK(expected == result);
}

BOOST_AUTO_TEST_CASE(async_server_multiple_parameters)
{
    std::uint8_t const result = test_simple_request_response<std::uint8_t>(
        [](async_test_interface &client, std::function<void(boost::system::error_code, std::uint8_t)> on_result)
        {
            client.real_multi_parameters("abc", 123, on_result);
        },
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 21, 'r', 'e', 'a', 'l', '_', 'm', 'u', 'l', 't', 'i', '_', 'p', 'a', 'r', 'a', 'm',
         'e', 't', 'e', 'r', 's', 3, 'a', 'b', 'c', 0, 123},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 126});
    std::uint8_t const expected{126};
    BOOST_CHECK_EQUAL(expected, result);
}

BOOST_AUTO_TEST_CASE(async_server_variant_first)
{
    Si::variant<std::uint16_t, std::string> const result =
        test_simple_request_response<Si::variant<std::uint16_t, std::string>>(
            [](async_test_interface &client,
               std::function<void(boost::system::error_code, Si::variant<std::uint16_t, std::string>)> on_result)
            {
                client.variant(static_cast<std::uint32_t>(0x11223344), on_result);
            },
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 'v', 'a', 'r', 'i', 'a', 'n', 't', 0, 0x11, 0x22, 0x33, 0x44},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x33, 0x45});
    Si::variant<std::uint16_t, std::string> const expected{static_cast<std::uint16_t>(0x3345)};
    BOOST_CHECK(expected == result);
}

BOOST_AUTO_TEST_CASE(async_server_variant_second)
{
    Si::variant<std::uint16_t, std::string> const result =
        test_simple_request_response<Si::variant<std::uint16_t, std::string>>(
            [](async_test_interface &client,
               std::function<void(boost::system::error_code, Si::variant<std::uint16_t, std::string>)> on_result)
            {
                client.variant(std::string("abc"), on_result);
            },
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 'v', 'a', 'r', 'i', 'a', 'n', 't', 1, 3, 'a', 'b', 'c'},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 'a', 'b', 'c', 'd'});
    Si::variant<std::uint16_t, std::string> const expected{std::string("abcd")};
    BOOST_CHECK(expected == result);
}

BOOST_DATA_TEST_CASE(async_server_integer, boost::unit_test::data::xrange<std::uint16_t>(1, 1001), number)
{
    std::uint16_t result = test_simple_request_response<std::uint16_t>(
        [number](async_test_interface &client, std::function<void(boost::system::error_code, std::uint16_t)> on_result)
        {
            client.atypical_int(number, on_result);
        },
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 'a', 't', 'y', 'p', 'i', 'c', 'a', 'l', '_', 'i', 'n', 't',
         static_cast<std::uint8_t>(number / 256), static_cast<std::uint8_t>(number % 256)},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, static_cast<std::uint8_t>(number / 256), static_cast<std::uint8_t>(number % 256)});
    BOOST_CHECK_EQUAL(number, result);
}
