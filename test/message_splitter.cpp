#include <boost/test/unit_test.hpp>
#include <warpcoil/cpp/message_splitter.hpp>
#include "test_streams.hpp"
#include "checkpoint.hpp"

BOOST_AUTO_TEST_CASE(message_splitter_wait_for_request_single)
{
    warpcoil::async_read_dummy_stream stream;
    warpcoil::cpp::message_splitter<warpcoil::async_read_dummy_stream> splitter(stream);
    BOOST_REQUIRE(!stream.respond);
    warpcoil::checkpoint got_request;
    splitter.wait_for_request(
        [&got_request](Si::error_or<std::tuple<warpcoil::request_id, std::string>> const request)
        {
            got_request.enter();
            BOOST_CHECK_EQUAL(123u, std::get<0>(request.get()));
            BOOST_CHECK_EQUAL("Method", std::get<1>(request.get()));
        });
    BOOST_REQUIRE(stream.respond);
    std::array<std::uint8_t, 16> const request = {
        {0, 0, 0, 0, 0, 0, 0, 0, 123, 6, 'M', 'e', 't', 'h', 'o', 'd'}};
    got_request.enable();
    Si::exchange(stream.respond, nullptr)(Si::make_memory_range(request));
    got_request.require_crossed();
    BOOST_REQUIRE(!stream.respond);
}

BOOST_AUTO_TEST_CASE(message_splitter_wait_for_request_and_response)
{
    warpcoil::async_read_dummy_stream stream;
    warpcoil::cpp::message_splitter<warpcoil::async_read_dummy_stream> splitter(stream);
    BOOST_REQUIRE(!stream.respond);
    warpcoil::checkpoint got_request;
    splitter.wait_for_request(
        [&got_request](Si::error_or<std::tuple<warpcoil::request_id, std::string>> const request)
        {
            got_request.enter();
            BOOST_CHECK_EQUAL(123u, std::get<0>(request.get()));
            BOOST_CHECK_EQUAL("Method", std::get<1>(request.get()));
        });
    BOOST_REQUIRE(stream.respond);
    warpcoil::checkpoint got_response;
    splitter.wait_for_response([&got_response](Si::error_or<warpcoil::request_id> const request)
                               {
                                   got_response.enter();
                                   BOOST_CHECK_EQUAL(99u, request.get());
                               });
    std::array<std::uint8_t, 16 + 9> const input = {
        {0, 0, 0, 0, 0, 0, 0, 0, 123, 6, 'M', 'e', 't', 'h', 'o', 'd', 1, 0, 0, 0, 0, 0, 0, 0, 99}};
    got_request.enable();
    got_response.enable();
    Si::exchange(stream.respond, nullptr)(Si::make_memory_range(input));
    got_request.require_crossed();
    got_response.require_crossed();
    BOOST_REQUIRE(!stream.respond);
}

BOOST_AUTO_TEST_CASE(message_splitter_wait_for_request_and_response_single_bytes)
{
    warpcoil::async_read_dummy_stream stream;
    warpcoil::cpp::message_splitter<warpcoil::async_read_dummy_stream> splitter(stream);
    BOOST_REQUIRE(!stream.respond);
    warpcoil::checkpoint got_request;
    splitter.wait_for_request(
        [&got_request](Si::error_or<std::tuple<warpcoil::request_id, std::string>> const request)
        {
            got_request.enter();
            BOOST_CHECK_EQUAL(123u, std::get<0>(request.get()));
            BOOST_CHECK_EQUAL("Method", std::get<1>(request.get()));
        });
    BOOST_REQUIRE(stream.respond);
    warpcoil::checkpoint got_response;
    splitter.wait_for_response([&got_response](Si::error_or<warpcoil::request_id> const request)
                               {
                                   got_response.enter();
                                   BOOST_CHECK_EQUAL(99u, request.get());
                               });
    const size_t request_size = 16;
    std::array<std::uint8_t, request_size + 9> const input = {
        {0, 0, 0, 0, 0, 0, 0, 0, 123, 6, 'M', 'e', 't', 'h', 'o', 'd', 1, 0, 0, 0, 0, 0, 0, 0, 99}};
    for (size_t i = 0; i < input.size(); ++i)
    {
        if (i == (request_size - 1))
        {
            got_request.enable();
        }
        if (i == (input.size() - 1))
        {
            got_response.enable();
        }
        Si::exchange(stream.respond,
                     nullptr)(Si::make_memory_range(input.data() + i, input.data() + i + 1));
        if (i == (request_size - 1))
        {
            got_request.require_crossed();
        }
        if (i == (input.size() - 1))
        {
            BOOST_REQUIRE(!stream.respond);
            got_response.require_crossed();
        }
        else
        {
            BOOST_REQUIRE(stream.respond);
        }
    }
}
