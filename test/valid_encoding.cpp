#include "test_streams.hpp"
#include "checkpoint.hpp"
#include "generated.hpp"
#include "boost_print_log_value.hpp"
#include <silicium/exchange.hpp>
#include <silicium/error_or.hpp>

namespace
{
    struct impl_test_interface : async_test_interface
    {
        void
        integer_sizes(std::tuple<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t> argument,
                      std::function<void(boost::system::error_code, std::vector<std::uint16_t>)> on_result) override
        {
            on_result({}, std::vector<std::uint16_t>{std::get<1>(argument)});
        }

        void no_result_no_parameter(std::tuple<> argument,
                                    std::function<void(boost::system::error_code, std::tuple<>)> on_result) override
        {
            on_result({}, argument);
        }

        void real_multi_parameters(std::string first, std::uint16_t second,
                                   std::function<void(boost::system::error_code, std::uint8_t)> on_result) override
        {
            on_result({}, static_cast<std::uint8_t>(first.size() + second));
        }

        void two_parameters(std::tuple<std::uint64_t, std::uint64_t> argument,
                            std::function<void(boost::system::error_code, std::uint64_t)> on_result) override
        {
            on_result({}, std::get<0>(argument) * std::get<1>(argument));
        }

        void two_results(
            std::uint64_t argument,
            std::function<void(boost::system::error_code, std::tuple<std::uint64_t, std::uint64_t>)> on_result) override
        {
            on_result({}, std::make_tuple(argument, argument));
        }

        void utf8(std::string argument, std::function<void(boost::system::error_code, std::string)> on_result) override
        {
            on_result({}, "Hello, " + argument + "!");
        }

        void vectors(std::vector<std::uint64_t> argument,
                     std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result) override
        {
            std::reverse(argument.begin(), argument.end());
            on_result({}, std::move(argument));
        }

        void vectors_256(std::vector<std::uint64_t> argument,
                         std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result) override
        {
            std::reverse(argument.begin(), argument.end());
            on_result({}, std::move(argument));
        }

        void atypical_int(std::uint16_t argument,
                          std::function<void(boost::system::error_code, std::uint16_t)> on_result) override
        {
            on_result({}, argument);
        }
    };

    template <class Result>
    Result test_simple_request_response(
        std::function<void(async_test_interface &, std::function<void(boost::system::error_code, Result)>)>
            begin_request,
        std::vector<std::uint8_t> expected_request, std::vector<std::uint8_t> expected_response)
    {
        impl_test_interface server_impl;
        warpcoil::async_read_stream server_requests;
        warpcoil::async_write_stream server_responses;
        async_test_interface_server<warpcoil::async_read_stream, warpcoil::async_write_stream> server(
            server_impl, server_requests, server_responses);
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
        async_test_interface_client<warpcoil::async_write_stream, warpcoil::async_read_stream> client(client_requests,
                                                                                                      client_responses);
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
        BOOST_REQUIRE(!client_responses.respond);
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
        {4, 'u', 't', 'f', '8', 4, 'N', 'a', 'm', 'e'},
        {12, 'H', 'e', 'l', 'l', 'o', ',', ' ', 'N', 'a', 'm', 'e', '!'});
    BOOST_CHECK_EQUAL("Hello, Name!", result);
}

BOOST_AUTO_TEST_CASE(async_server_vector)
{
    std::vector<std::uint64_t> result = test_simple_request_response<std::vector<std::uint64_t>>(
        [](async_test_interface &client,
           std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result)
        {
            client.vectors(std::vector<std::uint64_t>{3, 2, 1}, on_result);
        },
        {7, 'v', 'e', 'c', 't', 'o', 'r', 's', 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 2,
         0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3});
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
            {11, 't', 'w', 'o', '_', 'r', 'e', 's', 'u', 'l', 't', 's', 0, 0, 0, 0, 0, 0, 0, 123},
            {0, 0, 0, 0, 0, 0, 0, 123, 0, 0, 0, 0, 0, 0, 0, 123});
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
        {21, 'r', 'e', 'a', 'l', '_', 'm', 'u', 'l', 't', 'i', '_', 'p', 'a', 'r', 'a', 'm', 'e', 't', 'e', 'r', 's', 3,
         'a', 'b', 'c', 0, 123},
        {126});
    std::uint8_t const expected{126};
    BOOST_CHECK_EQUAL(expected, result);
}
