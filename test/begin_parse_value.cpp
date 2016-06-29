#include "generated.hpp"
#include "test_streams.hpp"
#include <boost/test/unit_test.hpp>
#include "checkpoint.hpp"

BOOST_AUTO_TEST_CASE(begin_parse_value_exact)
{
    warpcoil::async_read_stream stream;
    beast::streambuf buffer;
    warpcoil::checkpoint parse_completed;
    warpcoil::cpp::begin_parse_value(stream, buffer, warpcoil::cpp::integer_parser<std::uint32_t>(),
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
    BOOST_CHECK_EQUAL(0u, buffer.size());
}

BOOST_AUTO_TEST_CASE(begin_parse_value_empty_tuple)
{
    warpcoil::async_read_stream stream;
    beast::streambuf buffer;
    warpcoil::checkpoint parse_completed;
    parse_completed.enable();
    warpcoil::cpp::begin_parse_value(stream, buffer, warpcoil::cpp::tuple_parser<>(),
                                     [&parse_completed](boost::system::error_code const ec, std::tuple<> const)
                                     {
                                         parse_completed.enter();
                                         BOOST_REQUIRE(!ec);
                                     });
    parse_completed.require_crossed();
    BOOST_REQUIRE(!stream.respond);
    BOOST_CHECK_EQUAL(0u, buffer.size());
}

BOOST_AUTO_TEST_CASE(begin_parse_value_excess)
{
    warpcoil::async_read_stream stream;
    beast::streambuf buffer;
    {
        warpcoil::checkpoint parse_completed;
        warpcoil::cpp::begin_parse_value(
            stream, buffer, warpcoil::cpp::integer_parser<std::uint32_t>(),
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
    BOOST_REQUIRE_EQUAL(4u, buffer.size());
    BOOST_REQUIRE_EQUAL(0x99u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[0]);
    BOOST_REQUIRE_EQUAL(0x88u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[1]);
    BOOST_REQUIRE_EQUAL(0x77u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[2]);
    BOOST_REQUIRE_EQUAL(0x66u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[3]);
    {
        warpcoil::checkpoint parse_completed;
        parse_completed.enable();
        warpcoil::cpp::begin_parse_value(
            stream, buffer, warpcoil::cpp::integer_parser<std::uint32_t>(),
            [&parse_completed](boost::system::error_code const ec, std::uint32_t const parsed)
            {
                parse_completed.enter();
                BOOST_REQUIRE(!ec);
                BOOST_CHECK_EQUAL(0x99887766u, parsed);
            });
        parse_completed.require_crossed();
        BOOST_CHECK(!stream.respond);
    }
    BOOST_REQUIRE_EQUAL(0u, buffer.size());
}

BOOST_AUTO_TEST_CASE(begin_parse_value_immediate_completion)
{
    warpcoil::async_read_stream stream;
    ::beast::streambuf buffer;
    {
        std::array<std::uint8_t, 8> const initial_content = {{0xab, 0xcd, 0xef, 0x12, 0x99, 0x88, 0x77, 0x66}};
        auto to = buffer.prepare(initial_content.size());
        BOOST_REQUIRE_EQUAL(initial_content.size(), boost::asio::buffer_copy(to, boost::asio::buffer(initial_content)));
        buffer.commit(initial_content.size());
    }
    {
        warpcoil::checkpoint first_completed;
        warpcoil::checkpoint second_completed;
        first_completed.enable();
        warpcoil::cpp::begin_parse_value(
            stream, buffer, warpcoil::cpp::integer_parser<std::uint32_t>(),
            [&](boost::system::error_code const ec, std::uint32_t const parsed)
            {
                first_completed.enter();
                BOOST_REQUIRE(!ec);
                BOOST_CHECK_EQUAL(0xabcdef12u, parsed);
                BOOST_REQUIRE(!stream.respond);
                BOOST_REQUIRE_EQUAL(4u, buffer.size());
                BOOST_REQUIRE_EQUAL(0x99u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[0]);
                BOOST_REQUIRE_EQUAL(0x88u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[1]);
                BOOST_REQUIRE_EQUAL(0x77u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[2]);
                BOOST_REQUIRE_EQUAL(0x66u, boost::asio::buffer_cast<std::uint8_t const *>(*buffer.data().begin())[3]);
                second_completed.enable();
                warpcoil::cpp::begin_parse_value(stream, buffer, warpcoil::cpp::integer_parser<std::uint32_t>(),
                                                 [&](boost::system::error_code const ec, std::uint32_t const parsed)
                                                 {
                                                     second_completed.enter();
                                                     BOOST_REQUIRE(!ec);
                                                     BOOST_CHECK_EQUAL(0x99887766u, parsed);
                                                     BOOST_REQUIRE(!stream.respond);
                                                     BOOST_REQUIRE_EQUAL(0u, buffer.size());
                                                 });
                BOOST_REQUIRE(!stream.respond);
            });
        BOOST_REQUIRE(!stream.respond);
    }
    BOOST_CHECK_EQUAL(0u, buffer.size());
}
