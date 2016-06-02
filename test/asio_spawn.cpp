#include "generated.hpp"
#include "impl_test_interface.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/test/unit_test.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>

BOOST_AUTO_TEST_CASE(async_server_with_asio_spawn)
{
    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
    acceptor.listen();
    boost::asio::ip::tcp::socket accepted_socket(io);
    bool ok1 = false;
    warpcoil::impl_test_interface server_impl;
    acceptor.async_accept(
        accepted_socket, [&io, &accepted_socket, &ok1, &server_impl](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            boost::asio::spawn(
                io, [&server_impl, &accepted_socket, &ok1](boost::asio::yield_context yield)
                {
                    auto server_splitter =
                        std::make_shared<warpcoil::cpp::message_splitter<boost::asio::ip::tcp::socket>>(
                            accepted_socket);
                    auto writer =
                        std::make_shared<warpcoil::cpp::buffered_writer<boost::asio::ip::tcp::socket>>(accepted_socket);
                    writer->async_run([writer](boost::system::error_code const)
                                      {
                                          BOOST_FAIL("Unexpected error");
                                      });
                    async_test_interface_server<decltype(server_impl), boost::asio::ip::tcp::socket,
                                                boost::asio::ip::tcp::socket> server(server_impl, *server_splitter,
                                                                                     *writer);
                    server.serve_one_request(yield);
                    server.serve_one_request(yield);
                    BOOST_REQUIRE(!ok1);
                    ok1 = true;
                });
        });
    boost::asio::ip::tcp::socket socket(io);
    warpcoil::cpp::message_splitter<decltype(socket)> splitter(socket);
    warpcoil::cpp::buffered_writer<decltype(socket)> writer(socket);
    async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(writer, splitter);
    bool ok2 = false;
    socket.async_connect(
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
        [&io, &writer, &client, &ok2](boost::system::error_code ec)
        {
            BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
            writer.async_run([](boost::system::error_code const)
                             {
                                 BOOST_FAIL("Unexpected error");
                             });
            boost::asio::spawn(
                io, [&client, &ok2](boost::asio::yield_context yield)
                {
                    std::vector<std::uint64_t> result = client.vectors(std::vector<std::uint64_t>{12, 34, 56}, yield);
                    std::vector<std::uint64_t> const expected{56, 34, 12};
                    BOOST_CHECK(expected == result);

                    BOOST_CHECK_EQUAL("hello123", client.utf8("hello", yield));

                    BOOST_REQUIRE(!ok2);
                    ok2 = true;
                });
        });
    io.run();
    BOOST_CHECK(ok1);
    BOOST_CHECK(ok2);
}
