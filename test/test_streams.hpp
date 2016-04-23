#pragma once

#include <boost/test/unit_test.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/async_result.hpp>
#include <silicium/memory_range.hpp>
#include <silicium/error_or.hpp>

namespace warpcoil
{
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
}
