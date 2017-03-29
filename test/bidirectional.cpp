#include "generated.hpp"
#include "impl_test_interface.hpp"
#include "checkpoint.hpp"
#include "test_streams.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(bidirectional_with_sockets)
{
    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(
        io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::ip::tcp::socket accepted_socket(io);
    warpcoil::impl_test_interface server_impl_a;
    warpcoil::checkpoint request_answered_a;
    warpcoil::checkpoint request_answered_b;
    acceptor.async_accept(
        accepted_socket, [&accepted_socket, &server_impl_a, &request_answered_a,
                          &request_answered_b](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            auto splitter_a =
                std::make_shared<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>>(
                    accepted_socket);
            auto writer =
                std::make_shared<warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket>>(
                    accepted_socket);
            auto client_a = std::make_shared<async_test_interface_client<
                boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket>>(*writer, *splitter_a);
            auto server_a = std::make_shared<
                async_test_interface_server<decltype(server_impl_a), boost::asio::ip::tcp::socket,
                                            boost::asio::ip::tcp::socket>>(server_impl_a,
                                                                           *splitter_a, *writer);
            client_a->utf8("Y", [splitter_a, &request_answered_a, client_a, writer,
                                 server_a](Si::error_or<std::string> result)
                           {
                               BOOST_TEST_MESSAGE("Got answer on A");
                               request_answered_a.enter();
                               BOOST_CHECK_EQUAL("Y123", result.get());
                           });
            BOOST_TEST_MESSAGE("Sent request on A");
            BOOST_CHECK_EQUAL(1u, pending_requests(*client_a));
            server_a->serve_one_request(
                [server_a, splitter_a, writer, client_a](boost::system::error_code ec)
                {
                    BOOST_TEST_MESSAGE("Served request A");
                    BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                });
            request_answered_b.enable();
        });
    boost::asio::ip::tcp::socket connecting_socket(io);
    warpcoil::cpp::message_splitter<decltype(connecting_socket)> splitter_b(connecting_socket);
    warpcoil::impl_test_interface server_impl_b;
    warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket> writer(connecting_socket);
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket>
        client_b(writer, splitter_b);
    connecting_socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(),
                                       acceptor.local_endpoint().port()),
        [&client_b, &request_answered_a, &server_impl_b, &splitter_b, &writer, &connecting_socket,
         &request_answered_b](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            BOOST_CHECK_EQUAL(0u, pending_requests(client_b));
            auto server_b = std::make_shared<
                async_test_interface_server<decltype(server_impl_b), boost::asio::ip::tcp::socket,
                                            boost::asio::ip::tcp::socket>>(server_impl_b,
                                                                           splitter_b, writer);
            client_b.utf8("X", [&request_answered_b, server_b](Si::error_or<std::string> result)
                          {
                              BOOST_TEST_MESSAGE("Got answer on B");
                              request_answered_b.enter();
                              BOOST_CHECK_EQUAL("X123", result.get());
                          });
            BOOST_TEST_MESSAGE("Sent request on B");
            BOOST_CHECK_EQUAL(1u, pending_requests(client_b));
            server_b->serve_one_request([server_b](boost::system::error_code ec)
                                        {
                                            BOOST_TEST_MESSAGE("Served request B");
                                            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                        });
            request_answered_a.enable();
        });
    io.run();
    BOOST_CHECK_EQUAL(0u, pending_requests(client_b));
}

BOOST_AUTO_TEST_CASE(bidirectional)
{
    warpcoil::async_read_dummy_stream a_reading;
    warpcoil::async_write_dummy_stream a_writing;
    warpcoil::impl_test_interface server_impl_a;
    warpcoil::checkpoint request_answered_a;
    warpcoil::checkpoint request_answered_b;
    auto splitter_a =
        std::make_shared<warpcoil::cpp::message_splitter<decltype(a_reading)>>(a_reading);
    auto writer_a =
        std::make_shared<warpcoil::cpp::buffered_writer<decltype(a_writing)>>(a_writing);
    auto client_a =
        std::make_shared<async_test_interface_client<decltype(a_writing), decltype(a_reading)>>(
            *writer_a, *splitter_a);
    auto server_a =
        std::make_shared<async_test_interface_server<decltype(server_impl_a), decltype(a_reading),
                                                     decltype(a_writing)>>(server_impl_a,
                                                                           *splitter_a, *writer_a);
    client_a->utf8("Y", [splitter_a, &request_answered_a, client_a, writer_a,
                         server_a](Si::error_or<std::string> result)
                   {
                       BOOST_TEST_MESSAGE("Got answer on A");
                       request_answered_a.enter();
                       BOOST_CHECK_EQUAL("Y123", result.get());
                   });
    BOOST_TEST_MESSAGE("Sent request on A");
    BOOST_CHECK_EQUAL(1u, pending_requests(*client_a));
    server_a->serve_one_request(
        [server_a, splitter_a, writer_a, client_a](boost::system::error_code ec)
        {
            BOOST_TEST_MESSAGE("Served request A");
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
        });
    request_answered_b.enable();
    warpcoil::async_read_dummy_stream b_reading;
    warpcoil::async_write_dummy_stream b_writing;
    warpcoil::cpp::message_splitter<decltype(b_reading)> splitter_b(b_reading);
    warpcoil::impl_test_interface server_impl_b;
    warpcoil::cpp::buffered_writer<decltype(b_writing)> writer_b(b_writing);
    async_test_interface_client<decltype(b_writing), decltype(b_reading)> client_b(writer_b,
                                                                                   splitter_b);
    {
        BOOST_CHECK_EQUAL(0u, pending_requests(client_b));
        auto server_b =
            std::make_shared<async_test_interface_server<decltype(server_impl_b),
                                                         decltype(b_reading), decltype(b_writing)>>(
                server_impl_b, splitter_b, writer_b);
        client_b.utf8("X", [&request_answered_b, server_b](Si::error_or<std::string> result)
                      {
                          BOOST_TEST_MESSAGE("Got answer on B");
                          request_answered_b.enter();
                          BOOST_CHECK_EQUAL("X123", result.get());
                      });
        BOOST_TEST_MESSAGE("Sent request on B");
        BOOST_CHECK_EQUAL(1u, pending_requests(client_b));
        server_b->serve_one_request([server_b](boost::system::error_code ec)
                                    {
                                        BOOST_TEST_MESSAGE("Served request B");
                                        BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                    });
        request_answered_a.enable();
    }
    BOOST_REQUIRE_EQUAL(1u, a_writing.written.size());
    std::array<std::uint8_t, 16> const request_to_b = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'u', 't', 'f', '8', 1, 'Y'}};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(request_to_b.begin(), request_to_b.end(),
                                    a_writing.written[0].begin(), a_writing.written[0].end());
    a_writing.written.clear();
    BOOST_REQUIRE_EQUAL(1u, b_writing.written.size());
    std::array<std::uint8_t, 16> const request_to_a = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'u', 't', 'f', '8', 1, 'X'}};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(request_to_a.begin(), request_to_a.end(),
                                    b_writing.written[0].begin(), b_writing.written[0].end());
    b_writing.written.clear();
    BOOST_REQUIRE(a_reading.respond);
    BOOST_REQUIRE(b_reading.respond);
    Si::exchange(a_writing.handle_result, nullptr)(boost::system::error_code(), 16u);
    Si::exchange(b_writing.handle_result, nullptr)(boost::system::error_code(), 16u);
    Si::exchange(a_reading.respond, nullptr)(Si::make_memory_range(request_to_a));
    BOOST_REQUIRE(a_reading.respond);
    Si::exchange(b_reading.respond, nullptr)(Si::make_memory_range(request_to_b));
    BOOST_REQUIRE(b_reading.respond);
    std::array<std::uint8_t, 14> const response_to_a = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'Y', '1', '2', '3'}};
    std::array<std::uint8_t, 14> const response_to_b = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'X', '1', '2', '3'}};
    BOOST_REQUIRE_EQUAL(1u, a_writing.written.size());
    BOOST_REQUIRE_EQUAL_COLLECTIONS(response_to_b.begin(), response_to_b.end(),
                                    a_writing.written[0].begin(), a_writing.written[0].end());
    BOOST_REQUIRE_EQUAL(1u, b_writing.written.size());
    BOOST_REQUIRE_EQUAL_COLLECTIONS(response_to_a.begin(), response_to_a.end(),
                                    b_writing.written[0].begin(), b_writing.written[0].end());
    Si::exchange(a_writing.handle_result, nullptr)(boost::system::error_code(),
                                                   response_to_b.size());
    BOOST_REQUIRE_EQUAL(1u, pending_requests(*client_a));
    Si::exchange(a_reading.respond, nullptr)(Si::make_memory_range(response_to_a));
    BOOST_REQUIRE_EQUAL(0u, pending_requests(*client_a));
    Si::exchange(b_writing.handle_result, nullptr)(boost::system::error_code(),
                                                   response_to_a.size());
    BOOST_REQUIRE_EQUAL(1u, pending_requests(client_b));
    Si::exchange(b_reading.respond, nullptr)(Si::make_memory_range(response_to_b));
    BOOST_REQUIRE_EQUAL(0u, pending_requests(client_b));
    BOOST_CHECK(!a_reading.respond);
    BOOST_CHECK(!a_writing.handle_result);
    BOOST_CHECK(!b_reading.respond);
    BOOST_CHECK(!b_writing.handle_result);
    BOOST_REQUIRE_EQUAL(0u, pending_requests(*client_a));
    BOOST_REQUIRE_EQUAL(0u, pending_requests(client_b));
}

BOOST_AUTO_TEST_CASE(bidirectional_request_merged_with_response_regression_test)
{
    warpcoil::async_read_dummy_stream a_reading;
    warpcoil::async_write_dummy_stream a_writing;
    warpcoil::impl_test_interface server_impl_a;
    warpcoil::checkpoint request_answered_a;
    warpcoil::checkpoint request_answered_b;
    auto splitter_a =
        std::make_shared<warpcoil::cpp::message_splitter<decltype(a_reading)>>(a_reading);
    auto writer_a =
        std::make_shared<warpcoil::cpp::buffered_writer<decltype(a_writing)>>(a_writing);
    auto client_a =
        std::make_shared<async_test_interface_client<decltype(a_writing), decltype(a_reading)>>(
            *writer_a, *splitter_a);
    auto server_a =
        std::make_shared<async_test_interface_server<decltype(server_impl_a), decltype(a_reading),
                                                     decltype(a_writing)>>(server_impl_a,
                                                                           *splitter_a, *writer_a);
    client_a->utf8("Y", [splitter_a, &request_answered_a, client_a, writer_a,
                         server_a](Si::error_or<std::string> result)
                   {
                       BOOST_TEST_MESSAGE("Got answer on A");
                       request_answered_a.enter();
                       BOOST_CHECK_EQUAL("Y123", result.get());
                   });
    BOOST_TEST_MESSAGE("Sent request on A");
    BOOST_CHECK_EQUAL(1u, pending_requests(*client_a));
    server_a->serve_one_request(
        [server_a, splitter_a, writer_a, client_a](boost::system::error_code ec)
        {
            BOOST_TEST_MESSAGE("Served request A");
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
        });
    request_answered_b.enable();
    warpcoil::async_read_dummy_stream b_reading;
    warpcoil::async_write_dummy_stream b_writing;
    warpcoil::cpp::message_splitter<decltype(b_reading)> splitter_b(b_reading);
    warpcoil::impl_test_interface server_impl_b;
    warpcoil::cpp::buffered_writer<decltype(b_writing)> writer_b(b_writing);
    async_test_interface_client<decltype(b_writing), decltype(b_reading)> client_b(writer_b,
                                                                                   splitter_b);
    {
        BOOST_CHECK_EQUAL(0u, pending_requests(client_b));
        auto server_b =
            std::make_shared<async_test_interface_server<decltype(server_impl_b),
                                                         decltype(b_reading), decltype(b_writing)>>(
                server_impl_b, splitter_b, writer_b);
        client_b.utf8("X", [&request_answered_b, server_b](Si::error_or<std::string> result)
                      {
                          BOOST_TEST_MESSAGE("Got answer on B");
                          request_answered_b.enter();
                          BOOST_CHECK_EQUAL("X123", result.get());
                      });
        BOOST_TEST_MESSAGE("Sent request on B");
        BOOST_CHECK_EQUAL(1u, pending_requests(client_b));
        server_b->serve_one_request([server_b](boost::system::error_code ec)
                                    {
                                        BOOST_TEST_MESSAGE("Served request B");
                                        BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
                                    });
        request_answered_a.enable();
    }
    BOOST_REQUIRE_EQUAL(1u, a_writing.written.size());
    std::array<std::uint8_t, 16> const request_to_b = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'u', 't', 'f', '8', 1, 'Y'}};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(request_to_b.begin(), request_to_b.end(),
                                    a_writing.written[0].begin(), a_writing.written[0].end());
    a_writing.written.clear();
    BOOST_REQUIRE_EQUAL(1u, b_writing.written.size());
    std::array<std::uint8_t, 16> const request_to_a = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'u', 't', 'f', '8', 1, 'X'}};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(request_to_a.begin(), request_to_a.end(),
                                    b_writing.written[0].begin(), b_writing.written[0].end());
    b_writing.written.clear();
    BOOST_REQUIRE(a_reading.respond);
    BOOST_REQUIRE(b_reading.respond);
    Si::exchange(a_writing.handle_result, nullptr)(boost::system::error_code(), 16u);
    Si::exchange(b_writing.handle_result, nullptr)(boost::system::error_code(), 16u);
    Si::exchange(a_reading.respond, nullptr)(Si::make_memory_range(request_to_a));
    BOOST_REQUIRE(a_reading.respond);
    std::array<std::uint8_t, 16 + 14> everything_to_b;
    std::copy(request_to_b.begin(), request_to_b.end(), everything_to_b.begin());
    std::array<std::uint8_t, 14> const response_to_b = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'X', '1', '2', '3'}};
    std::copy(response_to_b.begin(), response_to_b.end(),
              everything_to_b.begin() + request_to_b.size());
    BOOST_REQUIRE_EQUAL(1u, pending_requests(client_b));
    Si::exchange(b_reading.respond, nullptr)(Si::make_memory_range(everything_to_b));
    BOOST_REQUIRE(!b_reading.respond);
    BOOST_REQUIRE_EQUAL(0u, pending_requests(client_b));
    std::array<std::uint8_t, 14> const response_to_a = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 'Y', '1', '2', '3'}};
    BOOST_REQUIRE_EQUAL(1u, a_writing.written.size());
    BOOST_REQUIRE_EQUAL_COLLECTIONS(response_to_b.begin(), response_to_b.end(),
                                    a_writing.written[0].begin(), a_writing.written[0].end());
    BOOST_REQUIRE_EQUAL(1u, b_writing.written.size());
    BOOST_REQUIRE_EQUAL_COLLECTIONS(response_to_a.begin(), response_to_a.end(),
                                    b_writing.written[0].begin(), b_writing.written[0].end());
    Si::exchange(a_writing.handle_result, nullptr)(boost::system::error_code(),
                                                   response_to_b.size());
    BOOST_REQUIRE_EQUAL(1u, pending_requests(*client_a));
    Si::exchange(a_reading.respond, nullptr)(Si::make_memory_range(response_to_a));
    BOOST_REQUIRE_EQUAL(0u, pending_requests(*client_a));
    Si::exchange(b_writing.handle_result, nullptr)(boost::system::error_code(),
                                                   response_to_a.size());
    BOOST_CHECK(!a_reading.respond);
    BOOST_CHECK(!a_writing.handle_result);
    BOOST_CHECK(!b_reading.respond);
    BOOST_CHECK(!b_writing.handle_result);
    BOOST_REQUIRE_EQUAL(0u, pending_requests(*client_a));
    BOOST_REQUIRE_EQUAL(0u, pending_requests(client_b));
}
