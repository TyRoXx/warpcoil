#include <boost/test/unit_test.hpp>
#include <warpcoil/cpp/message_splitter.hpp>
#include "test_streams.hpp"
#include "checkpoint.hpp"

BOOST_AUTO_TEST_CASE(message_splitter_wait_for_request_single)
{
    warpcoil::async_read_stream stream;
    warpcoil::cpp::message_splitter<warpcoil::async_read_stream> splitter(stream);
    BOOST_REQUIRE(!stream.respond);
    warpcoil::checkpoint got_request;
    splitter.wait_for_request(
        [&got_request](boost::system::error_code const ec, warpcoil::request_id const request, std::string const method)
        {
            got_request.enter();
            BOOST_REQUIRE(!ec);
            BOOST_CHECK_EQUAL(123u, request);
            BOOST_CHECK_EQUAL("Method", method);
        });
    BOOST_REQUIRE(stream.respond);
    std::array<std::uint8_t, 16> const request = {{0, 0, 0, 0, 0, 0, 0, 0, 123, 6, 'M', 'e', 't', 'h', 'o', 'd'}};
    got_request.enable();
    Si::exchange(stream.respond, nullptr)(Si::make_memory_range(request));
    got_request.require_crossed();
    BOOST_REQUIRE(!stream.respond);
}
