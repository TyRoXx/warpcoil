#include "generated.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/test_tools.hpp>
#include <silicium/exchange.hpp>
#include <silicium/error_or.hpp>

namespace
{
	struct impl_test_interface : async_test_interface
	{
		virtual void
		integer_sizes(std::tuple<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t> argument,
		              std::function<void(boost::system::error_code, std::vector<std::uint16_t>)> on_result) override
		{
			on_result({}, std::vector<std::uint16_t>{std::get<1>(argument)});
		}

		virtual void
		no_result_no_parameter(std::tuple<> argument,
		                       std::function<void(boost::system::error_code, std::tuple<>)> on_result) override
		{
			on_result({}, argument);
		}

		virtual void two_parameters(std::tuple<std::uint64_t, std::uint64_t> argument,
		                            std::function<void(boost::system::error_code, std::uint64_t)> on_result) override
		{
			on_result({}, std::get<0>(argument) * std::get<1>(argument));
		}

		virtual void two_results(
		    std::uint64_t argument,
		    std::function<void(boost::system::error_code, std::tuple<std::uint64_t, std::uint64_t>)> on_result) override
		{
			on_result({}, std::make_tuple(argument, argument));
		}

		virtual void utf8(std::string argument,
		                  std::function<void(boost::system::error_code, std::string)> on_result) override
		{
			on_result({}, "Hello, " + argument + "!");
		}

		virtual void
		vectors(std::vector<std::uint64_t> argument,
		        std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result) override
		{
			std::reverse(argument.begin(), argument.end());
			on_result({}, std::move(argument));
		}
	};

	struct async_write_stream
	{
		std::vector<std::vector<std::uint8_t>> written;
		std::function<void(boost::system::error_code, std::size_t)> handle_result;

		template <class ConstBufferSequence, class CompletionToken>
		auto async_write_some(ConstBufferSequence const &buffers, CompletionToken &&token)
		{
			BOOST_REQUIRE(!handle_result);
			using handler_type =
			    typename boost::asio::handler_type<decltype(token), void(boost::system::error_code, std::size_t)>::type;
			handler_type handler(std::forward<CompletionToken>(token));
			boost::asio::async_result<handler_type> result(handler);
			for (auto const &buffer : buffers)
			{
				std::uint8_t const *const data = boost::asio::buffer_cast<std::uint8_t const *>(buffer);
				written.emplace_back(std::vector<std::uint8_t>(data, data + boost::asio::buffer_size(buffer)));
			}
			handle_result = std::move(handler);
			return result.get();
		}
	};

	struct async_read_stream
	{
		std::function<void(Si::error_or<Si::memory_range>)> respond;

		template <class MutableBufferSequence, class CompletionToken>
		auto async_read_some(MutableBufferSequence const &buffers, CompletionToken &&token)
		{
			BOOST_REQUIRE(!respond);
			using handler_type =
			    typename boost::asio::handler_type<decltype(token), void(boost::system::error_code, std::size_t)>::type;
			handler_type handler(std::forward<CompletionToken>(token));
			boost::asio::async_result<handler_type> result(handler);
			respond = [ buffers, handler = std::move(handler) ](Si::error_or<Si::memory_range> response) mutable
			{
				if (response.is_error())
				{
					std::move(handler)(response.error(), 0);
				}
				else
				{
					std::size_t const bytes = boost::asio::buffer_copy(
					    buffers, boost::asio::buffer(response.get().begin(), response.get().size()));
					std::move(handler)({}, bytes);
				}
			};
			return result.get();
		}
	};

	struct checkpoint : private boost::noncopyable
	{
		checkpoint()
		    : m_state(state::created)
		{
		}

		~checkpoint()
		{
			BOOST_CHECK_EQUAL(state::crossed, m_state);
		}

		void enable()
		{
			BOOST_REQUIRE_EQUAL(state::created, m_state);
			m_state = state::enabled;
		}

		void enter()
		{
			BOOST_REQUIRE_EQUAL(state::enabled, m_state);
			m_state = state::crossed;
		}

		void require_crossed()
		{
			BOOST_REQUIRE_EQUAL(state::crossed, m_state);
		}

	private:
		enum class state
		{
			created,
			enabled,
			crossed
		};

		friend std::ostream &operator<<(std::ostream &out, state s)
		{
			switch (s)
			{
			case state::created:
				return out << "created";
			case state::enabled:
				return out << "enabled";
			case state::crossed:
				return out << "crossed";
			}
			SILICIUM_UNREACHABLE();
		}

		state m_state;
	};
}

namespace boost
{
	namespace test_tools
	{
		namespace tt_detail
		{
			template <typename T>
			struct print_log_value<std::vector<T>>
			{
				void operator()(std::ostream &out, std::vector<T> const &v)
				{
					out << '{';
					bool first = true;
					for (T const &e : v)
					{
						if (first)
						{
							first = false;
						}
						else
						{
							out << ", ";
						}
						out << static_cast<unsigned>(e);
					}
					out << '}';
				}
			};
		}
	}
}

namespace
{
	template <class Result>
	Result test_simple_request_response(
	    std::function<void(async_test_interface &, std::function<void(boost::system::error_code, Result)>)>
	        begin_request,
	    std::vector<std::uint8_t> expected_request, std::vector<std::uint8_t> expected_response)
	{
		impl_test_interface server_impl;
		async_read_stream server_requests;
		async_write_stream server_responses;
		async_test_interface_server<async_read_stream, async_write_stream> server(server_impl, server_requests,
		                                                                          server_responses);
		BOOST_REQUIRE(!server_requests.respond);
		BOOST_REQUIRE(!server_responses.handle_result);
		checkpoint request_served;
		server.serve_one_request([&request_served](boost::system::error_code ec)
		                         {
			                         request_served.enter();
			                         BOOST_REQUIRE(!ec);
			                     });
		BOOST_REQUIRE(server_requests.respond);
		BOOST_REQUIRE(!server_responses.handle_result);

		async_write_stream client_requests;
		async_read_stream client_responses;
		async_test_interface_client<async_write_stream, async_read_stream> client(client_requests, client_responses);
		BOOST_REQUIRE(!client_responses.respond);
		BOOST_REQUIRE(!client_requests.handle_result);

		checkpoint response_received;
		Result returned_result;
		async_type_erased_test_interface<decltype(client)> type_erased_client{client};
		begin_request(type_erased_client,
		              [&response_received, &returned_result](boost::system::error_code ec, Result result)
		              {
			              response_received.enter();
			              BOOST_REQUIRE(!ec);
			              returned_result = std::move(result);
			          });

		BOOST_REQUIRE(server_requests.respond);
		BOOST_REQUIRE(!server_responses.handle_result);
		BOOST_REQUIRE(server_responses.written.empty());
		BOOST_REQUIRE(!client_responses.respond);
		BOOST_REQUIRE(client_requests.handle_result);
		std::vector<std::vector<std::uint8_t>> const expected_request_buffers = {expected_request};
		BOOST_REQUIRE_EQUAL_COLLECTIONS(expected_request_buffers.begin(), expected_request_buffers.end(),
		                                client_requests.written.begin(), client_requests.written.end());
		Si::exchange(server_requests.respond, nullptr)(Si::make_memory_range(client_requests.written[0]));
		client_requests.written.clear();

		Si::exchange(client_requests.handle_result, nullptr)({}, expected_request.size());
		BOOST_REQUIRE(!client_requests.handle_result);

		BOOST_REQUIRE(!server_requests.respond);
		BOOST_REQUIRE(server_responses.handle_result);
		std::vector<std::vector<std::uint8_t>> const expected_response_buffers = {expected_response};
		BOOST_REQUIRE_EQUAL_COLLECTIONS(expected_response_buffers.begin(), expected_response_buffers.end(),
		                                server_responses.written.begin(), server_responses.written.end());
		request_served.enable();
		Si::exchange(server_responses.handle_result, nullptr)({}, expected_response.size());
		request_served.require_crossed();

		BOOST_REQUIRE(!server_requests.respond);
		BOOST_REQUIRE(!server_responses.handle_result);
		BOOST_REQUIRE(client_responses.respond);
		BOOST_REQUIRE(!client_requests.handle_result);

		response_received.enable();
		Si::exchange(client_responses.respond, nullptr)(Si::make_memory_range(expected_response));
		response_received.require_crossed();

		BOOST_REQUIRE(!server_requests.respond);
		BOOST_REQUIRE(!server_responses.handle_result);
		BOOST_REQUIRE(!client_responses.respond);
		BOOST_REQUIRE(!client_requests.handle_result);
		return returned_result;
	}
}

BOOST_AUTO_TEST_CASE(async_server_utf8)
{
	std::string result = test_simple_request_response<std::string>(
	    [](async_test_interface &client, std::function<void(boost::system::error_code, std::string)> on_result)
	    {
		    client.utf8("Name", on_result);
		},
	    {4, 'u', 't', 'f', '8', 4, 'N', 'a', 'm', 'e'},
	    {12, 'H', 'e', 'l', 'l', 'o', ',', ' ', 'N', 'a', 'm', 'e', '!'});
	BOOST_CHECK_EQUAL("Hello, Name!", result);
}

BOOST_AUTO_TEST_CASE(async_server_vector)
{
	std::vector<std::uint64_t> result = test_simple_request_response<std::vector<std::uint64_t>>(
	    [](async_test_interface &client,
	       std::function<void(boost::system::error_code, std::vector<std::uint64_t>)> on_result)
	    {
		    client.vectors(std::vector<std::uint64_t>{3, 2, 1}, on_result);
		},
	    {7, 'v', 'e', 'c', 't', 'o', 'r', 's', 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 2,
	     0, 0, 0, 0, 0, 0, 0, 1},
	    {0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3});
	std::vector<std::uint64_t> const expected = {1, 2, 3};
	BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), result.begin(), result.end());
}

BOOST_AUTO_TEST_CASE(async_server_tuple)
{
	std::tuple<std::uint64_t, std::uint64_t> result =
	    test_simple_request_response<std::tuple<std::uint64_t, std::uint64_t>>(
	        [](async_test_interface &client,
	           std::function<void(boost::system::error_code, std::tuple<std::uint64_t, std::uint64_t>)> on_result)
	        {
		        client.two_results(123, on_result);
		    },
	        {11, 't', 'w', 'o', '_', 'r', 'e', 's', 'u', 'l', 't', 's', 0, 0, 0, 0, 0, 0, 0, 123},
	        {0, 0, 0, 0, 0, 0, 0, 123, 0, 0, 0, 0, 0, 0, 0, 123});
	std::tuple<std::uint64_t, std::uint64_t> const expected{123, 123};
	BOOST_CHECK(expected == result);
}
