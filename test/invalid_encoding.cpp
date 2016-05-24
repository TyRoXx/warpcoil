#include "test_streams.hpp"
#include "checkpoint.hpp"
#include "generated.hpp"
#include "boost_print_log_value.hpp"
#include <silicium/exchange.hpp>
#include <silicium/error_or.hpp>

namespace
{
    struct failing_test_interface : async_test_interface
    {
        void
        integer_sizes(std::tuple<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t> argument,
                      std::function<void(boost::system::error_code, std::vector<std::uint16_t>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void no_result_no_parameter(std::tuple<> argument,
                                    std::function<void(boost::system::error_code, std::tuple<>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void real_multi_parameters(std::string first, std::uint16_t second,
                                   std::function<void(boost::system::error_code, std::uint8_t)> on_result) override
        {
            Si::ignore_unused_variable_warning(first);
            Si::ignore_unused_variable_warning(second);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void two_parameters(std::tuple<std::uint64_t, std::uint64_t> argument,
                            std::function<void(boost::system::error_code, std::uint64_t)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void two_results(
            std::uint64_t argument,
            std::function<void(boost::system::error_code, std::tuple<std::uint64_t, std::uint64_t>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void utf8(std::string argument, std::function<void(boost::system::error_code, std::string)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void vectors(std::vector<std::uint64_t> argument,
                     std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void vectors_256(std::vector<std::uint64_t> argument,
                         std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        void atypical_int(std::uint16_t argument,
                          std::function<void(boost::system::error_code, std::uint16_t)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }

        virtual void variant(
            Si::variant<std::uint32_t, std::string> argument,
            std::function<void(boost::system::error_code, Si::variant<std::uint16_t, std::string>)> on_result) override
        {
            Si::ignore_unused_variable_warning(argument);
            Si::ignore_unused_variable_warning(on_result);
            BOOST_FAIL("unexpected call");
        }
    };

    void test_invalid_server_request(std::vector<std::uint8_t> expected_request)
    {
        failing_test_interface server_impl;
        warpcoil::async_read_stream server_requests;
        warpcoil::async_write_stream server_responses;
        async_test_interface_server<decltype(server_impl), warpcoil::async_read_stream, warpcoil::async_write_stream>
            server(server_impl, server_requests, server_responses);
        BOOST_REQUIRE(!server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);
        warpcoil::checkpoint request_served;
        server.serve_one_request([&request_served](boost::system::error_code ec)
                                 {
                                     request_served.enter();
                                     BOOST_CHECK_EQUAL(warpcoil::cpp::make_invalid_input_error(), ec);
                                 });
        BOOST_REQUIRE(server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);
        BOOST_REQUIRE(server_responses.written.empty());
        request_served.enable();
        Si::exchange(server_requests.respond, nullptr)(Si::make_memory_range(expected_request));
        request_served.require_crossed();
        BOOST_REQUIRE(!server_requests.respond);
        BOOST_REQUIRE(!server_responses.handle_result);
    }
}

BOOST_AUTO_TEST_CASE(async_server_invalid_utf8_request)
{
    test_invalid_server_request({0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'u', 't', 'f', '8', 5, 'N', 'a', 'm', 'e', 0xff});
}

BOOST_AUTO_TEST_CASE(async_server_invalid_variant_request_a)
{
    test_invalid_server_request({0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 'v', 'a', 'r', 'i', 'a', 'n', 't', 2, 0, 0, 0, 0});
}

BOOST_AUTO_TEST_CASE(async_server_invalid_variant_request_b)
{
    test_invalid_server_request({0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 'v', 'a', 'r', 'i', 'a', 'n', 't', 255, 0, 0, 0, 0});
}

BOOST_AUTO_TEST_CASE(async_server_invalid_message_type)
{
    test_invalid_server_request({1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'u', 't', 'f', '8', 4, 'N', 'a', 'm', 'e'});
}

namespace
{
    template <class Result>
    void test_invalid_client_request(
        std::function<void(async_test_interface &, std::function<void(boost::system::error_code, Result)>)>
            begin_request)
    {
        warpcoil::async_write_stream client_requests;
        warpcoil::async_read_stream client_responses;
        async_test_interface_client<warpcoil::async_write_stream, warpcoil::async_read_stream> client(client_requests,
                                                                                                      client_responses);
        BOOST_REQUIRE(!client_responses.respond);
        BOOST_REQUIRE(!client_requests.handle_result);
        async_type_erased_test_interface<decltype(client)> type_erased_client{client};
        warpcoil::checkpoint request_rejected;
        request_rejected.enable();
        begin_request(type_erased_client, [&request_rejected](boost::system::error_code ec, Result result)
                      {
                          BOOST_REQUIRE_EQUAL(warpcoil::cpp::make_invalid_input_error(), ec);
                          Si::ignore_unused_variable_warning(result);
                          request_rejected.enter();
                      });
        request_rejected.require_crossed();
        BOOST_REQUIRE(!client_responses.respond);
        BOOST_REQUIRE(!client_requests.handle_result);
    }
}

BOOST_AUTO_TEST_CASE(async_client_invalid_utf8_request)
{
    test_invalid_client_request<std::string>(
        [](async_test_interface &client, std::function<void(boost::system::error_code, std::string)> on_result)
        {
            client.utf8("Name\xff", on_result);
        });
}

BOOST_AUTO_TEST_CASE(async_client_invalid_utf8_length_request)
{
    test_invalid_client_request<std::string>(
        [](async_test_interface &client, std::function<void(boost::system::error_code, std::string)> on_result)
        {
            client.utf8(std::string(256, 'a'), on_result);
        });
}

BOOST_AUTO_TEST_CASE(async_client_invalid_vector_length_request)
{
    test_invalid_client_request<std::vector<std::uint64_t>>(
        [](async_test_interface &client,
           std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result)
        {
            client.vectors_256(std::vector<std::uint64_t>(256), on_result);
        });
}

BOOST_AUTO_TEST_CASE(async_client_invalid_int_request_too_small)
{
    test_invalid_client_request<std::uint16_t>(
        [](async_test_interface &client, std::function<void(boost::system::error_code, std::uint16_t)> on_result)
        {
            client.atypical_int(0, on_result);
        });
}

BOOST_AUTO_TEST_CASE(async_client_invalid_int_request_too_large)
{
    test_invalid_client_request<std::uint16_t>(
        [](async_test_interface &client, std::function<void(boost::system::error_code, std::uint16_t)> on_result)
        {
            client.atypical_int(1001, on_result);
        });
}

BOOST_AUTO_TEST_CASE(async_client_invalid_variant_element_request)
{
    test_invalid_client_request<Si::variant<std::uint16_t, std::string>>(
        [](async_test_interface &client,
           std::function<void(boost::system::error_code, Si::variant<std::uint16_t, std::string>)> on_result)
        {
            client.variant(std::string("Name\xff"), on_result);
        });
}
