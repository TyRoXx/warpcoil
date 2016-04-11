#include "generated.hpp"
#include <boost/test/unit_test.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/source/memory_source.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>

BOOST_AUTO_TEST_CASE(async_client_with_callback)
{
	boost::asio::io_service io;
	boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
	acceptor.listen();
	boost::asio::ip::tcp::socket accepted_socket(io);
	std::array<std::uint8_t, 1024> receive_buffer;
	bool ok1 = false;
	acceptor.async_accept(accepted_socket, [&accepted_socket, &receive_buffer, &ok1](boost::system::error_code ec)
	                      {
		                      BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
		                      accepted_socket.async_receive(
		                          boost::asio::buffer(receive_buffer),
		                          [&accepted_socket, &ok1](boost::system::error_code ec, std::size_t received)
		                          {
			                          BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
			                          BOOST_REQUIRE_EQUAL(23, received);
			                          BOOST_REQUIRE(!ok1);
			                          ok1 = true;
			                      });
		                  });
	boost::asio::ip::tcp::socket socket(io);
	async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(socket, socket);
	bool ok2 = false;
	socket.async_connect(
	    boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
	    [&client, &ok2](boost::system::error_code ec)
	    {
		    BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
		    client.no_result_no_parameter(std::tuple<>(), [&ok2](boost::system::error_code ec, std::tuple<>)
		                                  {
			                                  BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
			                                  BOOST_REQUIRE(!ok2);
			                                  ok2 = true;
			                              });
		});
	io.run();
	BOOST_CHECK(ok1);
	BOOST_CHECK(ok2);
}

BOOST_AUTO_TEST_CASE(async_client_with_asio_spawn)
{
	boost::asio::io_service io;
	boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
	acceptor.listen();
	boost::asio::ip::tcp::socket accepted_socket(io);
	std::array<std::uint8_t, 1024> receive_buffer;
	bool ok1 = false;
	acceptor.async_accept(accepted_socket, [&accepted_socket, &receive_buffer, &ok1](boost::system::error_code ec)
	                      {
		                      BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
		                      accepted_socket.async_receive(
		                          boost::asio::buffer(receive_buffer),
		                          [&accepted_socket, &ok1](boost::system::error_code ec, std::size_t received)
		                          {
			                          BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
			                          BOOST_REQUIRE_EQUAL(23, received);
			                          BOOST_REQUIRE(!ok1);
			                          ok1 = true;
			                      });
		                  });
	boost::asio::ip::tcp::socket socket(io);
	async_test_interface_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(socket, socket);
	bool ok2 = false;
	socket.async_connect(
	    boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
	    [&io, &client, &ok2](boost::system::error_code ec)
	    {
		    BOOST_REQUIRE_EQUAL(boost::system::error_code(), ec);
		    boost::asio::spawn(io, [&client, &ok2](boost::asio::yield_context yield)
		                       {
			                       std::tuple<> result = client.no_result_no_parameter(std::tuple<>(), yield);
			                       Si::ignore_unused_variable_warning(result);
			                       BOOST_REQUIRE(!ok2);
			                       ok2 = true;
			                   });
		});
	io.run();
	BOOST_CHECK(ok1);
	BOOST_CHECK(ok2);
}
