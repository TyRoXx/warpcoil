#include "generated.hpp"
#include <boost/test/unit_test.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <boost/asio/ip/tcp.hpp>

BOOST_AUTO_TEST_CASE(test_async)
{
	boost::asio::io_service io;
	boost::asio::ip::tcp::acceptor acceptor(
	    io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0),
	    true);
	acceptor.listen();
	boost::asio::ip::tcp::socket accepted_socket(io);
	std::array<std::uint8_t, 1024> receive_buffer;
	acceptor.async_accept(
	    accepted_socket,
	    [&accepted_socket, &receive_buffer](boost::system::error_code ec)
	    {
		    BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
		    accepted_socket.async_receive(
		        boost::asio::buffer(receive_buffer),
		        [&accepted_socket](boost::system::error_code ec,
		                           std::size_t received)
		        {
			        BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
			        BOOST_REQUIRE_EQUAL(23, received);
			        boost::asio::async_write(
			            accepted_socket, boost::asio::buffer("something", 9),
			            [](boost::system::error_code ec, std::size_t)
			            {
				            BOOST_REQUIRE_EQUAL(boost::system::error_code(),
				                                ec);
				        });
			    });
		});
	boost::asio::ip::tcp::socket socket(io);
	async_test_interface_client<boost::asio::ip::tcp::socket,
	                            boost::asio::ip::tcp::socket> client(socket,
	                                                                 socket);
	bool ok = false;
	socket.async_connect(
	    boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(),
	                                   acceptor.local_endpoint().port()),
	    [&client, &ok](boost::system::error_code ec)
	    {
		    BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
		    client.no_result_no_parameter(
		        std::tuple<>(),
		        [&ok](boost::system::error_code ec, std::tuple<>)
		        {
			        BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
			        BOOST_REQUIRE(!ok);
			        ok = true;
			    });
		});
	io.run();
	BOOST_CHECK(ok);
}
