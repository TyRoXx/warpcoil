#pragma once

#include <boost/test/unit_test.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/async_result.hpp>
#include <silicium/memory_range.hpp>
#include <silicium/error_or.hpp>
#include <boost/asio/io_service.hpp>
#include <beast/websocket/teardown.hpp>

namespace warpcoil
{
    struct async_write_dummy_stream
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

    struct async_read_dummy_stream
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
                    BOOST_REQUIRE_EQUAL(static_cast<size_t>(response.get().size()), bytes);
                    std::move(handler)({}, bytes);
                }
            };
            return result.get();
        }
    };

    struct async_read_write_dummy_stream : async_write_dummy_stream, async_read_dummy_stream
    {
        boost::asio::io_service &io;

        explicit async_read_write_dummy_stream(boost::asio::io_service &io)
            : io(io)
        {
        }

        boost::asio::io_service &get_io_service() const
        {
            return io;
        }
    };
}

namespace beast
{
    namespace websocket
    {
        template <class TeardownHandler>
        void async_teardown(beast::websocket::teardown_tag, warpcoil::async_read_write_dummy_stream &stream,
                            TeardownHandler &&handler)
        {
            stream.get_io_service().post([handler = std::forward<TeardownHandler>(handler)]() mutable
                                         {
                                             std::forward<TeardownHandler>(handler)(boost::system::error_code());
                                         });
        }
    }
}
