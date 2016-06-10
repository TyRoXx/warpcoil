#include <boost/test/unit_test.hpp>
#include <warpcoil/cpp/buffered_writer.hpp>
#include "test_streams.hpp"
#include <silicium/sink/append.hpp>
#include <silicium/exchange.hpp>

BOOST_AUTO_TEST_CASE(buffered_writer_single_send)
{
    warpcoil::async_write_stream stream;
    warpcoil::cpp::buffered_writer<warpcoil::async_write_stream> buffered(stream);
    BOOST_REQUIRE(!stream.handle_result);
    buffered.async_run([](boost::system::error_code const)
                       {
                           BOOST_FAIL("Unexpected error");
                       });
    BOOST_REQUIRE(!stream.handle_result);
    {
        auto sink = buffered.buffer_sink();
        BOOST_REQUIRE(!stream.handle_result);
        Si::append(sink, 123);
        Si::append(sink, 4);
    }
    buffered.send_buffer([](boost::system::error_code const ec)
                         {
                             BOOST_REQUIRE(!ec);
                         });
    BOOST_REQUIRE(stream.handle_result);
    {
        std::array<std::uint8_t, 2> const expected = {{123, 4}};
        BOOST_REQUIRE_EQUAL(1u, stream.written.size());
        BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), stream.written[0].begin(),
                                      stream.written[0].end());
    }
    Si::exchange(stream.handle_result, nullptr)({}, 2);
    BOOST_REQUIRE(!stream.handle_result);
}

BOOST_AUTO_TEST_CASE(buffered_writer_buffer_during_send)
{
    warpcoil::async_write_stream stream;
    warpcoil::cpp::buffered_writer<warpcoil::async_write_stream> buffered(stream);
    BOOST_REQUIRE(!stream.handle_result);
    buffered.async_run([](boost::system::error_code const)
                       {
                           BOOST_FAIL("Unexpected error");
                       });
    BOOST_REQUIRE(!stream.handle_result);
    {
        auto sink = buffered.buffer_sink();
        BOOST_REQUIRE(!stream.handle_result);
        Si::append(sink, 123);
        Si::append(sink, 4);
    }
    buffered.send_buffer([](boost::system::error_code const ec)
                         {
                             BOOST_REQUIRE(!ec);
                         });
    BOOST_REQUIRE(stream.handle_result);
    {
        std::array<std::uint8_t, 2> const expected = {{123, 4}};
        BOOST_REQUIRE_EQUAL(1u, stream.written.size());
        BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), stream.written[0].begin(),
                                      stream.written[0].end());
    }
    {
        auto sink = buffered.buffer_sink();
        Si::append(sink, 17);
    }
    stream.written.clear();
    Si::exchange(stream.handle_result, nullptr)({}, 2);
    BOOST_REQUIRE(stream.handle_result);
    {
        std::array<std::uint8_t, 1> const expected = {{17}};
        BOOST_REQUIRE_EQUAL(1u, stream.written.size());
        BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), stream.written[0].begin(),
                                      stream.written[0].end());
    }
    Si::exchange(stream.handle_result, nullptr)({}, 1);
    BOOST_REQUIRE(!stream.handle_result);
}

BOOST_AUTO_TEST_CASE(buffered_writer_queuing)
{
    warpcoil::async_write_stream stream;
    warpcoil::cpp::buffered_writer<warpcoil::async_write_stream> buffered(stream);
    BOOST_REQUIRE(!stream.handle_result);
    buffered.async_run([](boost::system::error_code const)
                       {
                           BOOST_FAIL("Unexpected error");
                       });
    BOOST_REQUIRE(!stream.handle_result);
    std::vector<std::uint8_t> expected;
    std::size_t number_of_callbacks = 0;
    for (std::uint8_t i = 23; i < 34; ++i)
    {
        {
            auto sink = buffered.buffer_sink();
            Si::append(sink, i);
            expected.push_back(i);
        }
        buffered.send_buffer([&number_of_callbacks](boost::system::error_code const ec)
                             {
                                 ++number_of_callbacks;
                                 BOOST_REQUIRE(!ec);
                             });
        BOOST_REQUIRE(stream.handle_result);
    }
    BOOST_REQUIRE_EQUAL(1u, stream.written.size());
    BOOST_REQUIRE_EQUAL(expected[0], stream.written[0][0]);
    stream.written.clear();
    Si::exchange(stream.handle_result, nullptr)({}, 1);
    BOOST_REQUIRE_EQUAL(1u, stream.written.size());
    BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin() + 1, expected.end(), stream.written[0].begin(),
                                  stream.written[0].end());
    Si::exchange(stream.handle_result, nullptr)({}, expected.size() - 1);
    BOOST_REQUIRE(!stream.handle_result);
    BOOST_CHECK_EQUAL(expected.size(), number_of_callbacks);
}
