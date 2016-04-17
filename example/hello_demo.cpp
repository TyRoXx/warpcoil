#include "hello_interfaces.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <silicium/error_or.hpp>
#include <iostream>

namespace server
{
	struct my_hello_service : async_hello_as_a_service
	{
		virtual void
		type_erased_hello(std::vector<std::uint8_t> argument,
		                  std::function<void(boost::system::error_code, std::vector<std::uint8_t>)> on_result) override
		{
			std::vector<std::uint8_t> result;
			auto result_writer = Si::Sink<std::uint8_t, Si::success>::erase(Si::make_container_sink(result));
			Si::append(result_writer, Si::make_c_str_range(reinterpret_cast<std::uint8_t const *>("Hello, ")));
			Si::append(result_writer, Si::make_contiguous_range(argument));
			Si::append(result_writer, Si::make_c_str_range(reinterpret_cast<std::uint8_t const *>("!")));
			on_result({}, std::move(result));
		}
	};
}

int main()
{
	boost::asio::io_service io;
	boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0), true);
	acceptor.listen();
	boost::asio::ip::tcp::socket accepted_socket(io);
	server::my_hello_service server_impl;
	acceptor.async_accept(
	    accepted_socket, [&accepted_socket, &server_impl](boost::system::error_code ec)
	    {
		    Si::throw_if_error(ec);
		    auto server = std::make_shared<
		        async_hello_as_a_service_server<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket>>(
		        server_impl, accepted_socket, accepted_socket);
		    server->serve_one_request([server](boost::system::error_code ec)
		                              {
			                              Si::throw_if_error(ec);
			                          });
		});
	boost::asio::ip::tcp::socket socket(io);
	async_hello_as_a_service_client<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> client(socket, socket);
	socket.async_connect(
	    boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()),
	    [&io, &client](boost::system::error_code ec)
	    {
		    Si::throw_if_error(ec);
		    std::vector<std::uint8_t> argument;
		    std::string name;
		    std::cin >> name;
		    std::uint8_t const *const name_data = reinterpret_cast<std::uint8_t const *>(name.data());
		    Si::append(Si::make_container_sink(argument), Si::make_iterator_range(name_data, name_data + name.size()));
		    client.hello(std::move(argument), [](boost::system::error_code ec, std::vector<std::uint8_t> result)
		                 {
			                 Si::throw_if_error(ec);
			                 std::cout.write(reinterpret_cast<char const *>(result.data()), result.size());
			                 std::cout << '\n';
			             });
		});
	io.run();
}
