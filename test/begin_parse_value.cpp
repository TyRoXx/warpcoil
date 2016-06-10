#include "generated.hpp"
#include "test_streams.hpp"
#include <boost/test/unit_test.hpp>
#include "checkpoint.hpp"

BOOST_AUTO_TEST_CASE(begin_parse_value_exact)
{
    warpcoil::async_read_stream stream;
    std::array<std::uint8_t, 100> buffer;
    std::size_t buffer_used = 0;
    warpcoil::checkpoint parse_completed;
    warpcoil::cpp::begin_parse_value(stream, boost::asio::buffer(buffer), buffer_used,
                                     warpcoil::cpp::integer_parser<std::uint32_t>(),
                                     [&parse_completed](boost::system::error_code const ec, std::uint32_t const parsed)
                                     {
                                         parse_completed.enter();
                                         BOOST_REQUIRE(!ec);
                                         BOOST_CHECK_EQUAL(0xabcdef12u, parsed);
                                     });
    BOOST_REQUIRE(stream.respond);
    parse_completed.enable();
    std::array<std::uint8_t, 4> const input = {{0xab, 0xcd, 0xef, 0x12}};
    Si::exchange(stream.respond, nullptr)(Si::make_memory_range(input));
    parse_completed.require_crossed();
    BOOST_REQUIRE(!stream.respond);
    BOOST_CHECK_EQUAL(0u, buffer_used);
}

BOOST_AUTO_TEST_CASE(begin_parse_value_excess)
{
    warpcoil::async_read_stream stream;
    std::array<std::uint8_t, 100> buffer;
    std::size_t buffer_used = 0;
    {
        warpcoil::checkpoint parse_completed;
        warpcoil::cpp::begin_parse_value(
            stream, boost::asio::buffer(buffer), buffer_used, warpcoil::cpp::integer_parser<std::uint32_t>(),
            [&parse_completed](boost::system::error_code const ec, std::uint32_t const parsed)
            {
                parse_completed.enter();
                BOOST_REQUIRE(!ec);
                BOOST_CHECK_EQUAL(0xabcdef12u, parsed);
            });
        BOOST_REQUIRE(stream.respond);
        parse_completed.enable();
        std::array<std::uint8_t, 8> const input = {{0xab, 0xcd, 0xef, 0x12, 0x99, 0x88, 0x77, 0x66}};
        Si::exchange(stream.respond, nullptr)(Si::make_memory_range(input));
        parse_completed.require_crossed();
    }
    BOOST_REQUIRE(!stream.respond);
    BOOST_REQUIRE_EQUAL(4u, buffer_used);
    BOOST_REQUIRE_EQUAL(0x99u, buffer[0]);
    BOOST_REQUIRE_EQUAL(0x88u, buffer[1]);
    BOOST_REQUIRE_EQUAL(0x77u, buffer[2]);
    BOOST_REQUIRE_EQUAL(0x66u, buffer[3]);
    {
        warpcoil::checkpoint parse_completed;
        parse_completed.enable();
        warpcoil::cpp::begin_parse_value(
            stream, boost::asio::buffer(buffer), buffer_used, warpcoil::cpp::integer_parser<std::uint32_t>(),
            [&parse_completed](boost::system::error_code const ec, std::uint32_t const parsed)
            {
                parse_completed.enter();
                BOOST_REQUIRE(!ec);
                BOOST_CHECK_EQUAL(0x99887766u, parsed);
            });
        parse_completed.require_crossed();
        BOOST_CHECK(!stream.respond);
    }
    BOOST_REQUIRE_EQUAL(0u, buffer_used);
}

BOOST_AUTO_TEST_CASE(begin_parse_value_immediate_completion)
{
    warpcoil::async_read_stream stream;
    std::array<std::uint8_t, 8> buffer = {{0xab, 0xcd, 0xef, 0x12, 0x99, 0x88, 0x77, 0x66}};
    std::size_t buffer_used = buffer.size();
    {
        warpcoil::checkpoint first_completed;
        warpcoil::checkpoint second_completed;
        first_completed.enable();
        warpcoil::cpp::begin_parse_value(
            stream, boost::asio::buffer(buffer), buffer_used, warpcoil::cpp::integer_parser<std::uint32_t>(),
            [&](boost::system::error_code const ec, std::uint32_t const parsed)
            {
                first_completed.enter();
                BOOST_REQUIRE(!ec);
                BOOST_CHECK_EQUAL(0xabcdef12u, parsed);
                BOOST_REQUIRE(!stream.respond);
                BOOST_REQUIRE_EQUAL(4u, buffer_used);
                BOOST_REQUIRE_EQUAL(0x99u, buffer[0]);
                BOOST_REQUIRE_EQUAL(0x88u, buffer[1]);
                BOOST_REQUIRE_EQUAL(0x77u, buffer[2]);
                BOOST_REQUIRE_EQUAL(0x66u, buffer[3]);
                second_completed.enable();
                warpcoil::cpp::begin_parse_value(stream, boost::asio::buffer(buffer), buffer_used,
                                                 warpcoil::cpp::integer_parser<std::uint32_t>(),
                                                 [&](boost::system::error_code const ec, std::uint32_t const parsed)
                                                 {
                                                     second_completed.enter();
                                                     BOOST_REQUIRE(!ec);
                                                     BOOST_CHECK_EQUAL(0x99887766u, parsed);
                                                     BOOST_REQUIRE(!stream.respond);
                                                     BOOST_REQUIRE_EQUAL(0u, buffer_used);
                                                 });
                BOOST_REQUIRE(!stream.respond);
            });
        BOOST_REQUIRE(!stream.respond);
    }
    BOOST_CHECK_EQUAL(0u, buffer_used);
}
