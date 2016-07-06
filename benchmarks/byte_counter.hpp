#include <boost/asio/async_result.hpp>
#include <cstdint>

namespace warpcoil
{
    template <class AsyncStream>
    struct byte_counter
    {
        AsyncStream underlying;
        std::uint64_t written = 0;
        std::uint64_t read = 0;

        template <class... Args>
        explicit byte_counter(Args &&... args)
            : underlying(std::forward<Args>(args)...)
        {
        }

        template <class ConstBufferSequence, class CompletionToken>
        auto async_write_some(ConstBufferSequence const &buffers, CompletionToken &&token)
        {
            using handler_type =
                typename boost::asio::handler_type<decltype(token), void(boost::system::error_code, std::size_t)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            underlying.async_write_some(buffers, warpcoil::cpp::wrap_handler(
                                                     [this](boost::system::error_code ec, std::size_t bytes_transferred,
                                                            decltype(handler) &handler)
                                                     {
                                                         written += bytes_transferred;
                                                         handler(ec, bytes_transferred);
                                                     },
                                                     std::move(handler)));
            return result.get();
        }

        template <class MutableBufferSequence, class CompletionToken>
        auto async_read_some(MutableBufferSequence const &buffers, CompletionToken &&token)
        {
            using handler_type =
                typename boost::asio::handler_type<decltype(token), void(boost::system::error_code, std::size_t)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            underlying.async_read_some(buffers, warpcoil::cpp::wrap_handler(
                                                    [this](boost::system::error_code ec, std::size_t bytes_transferred,
                                                           decltype(handler) &handler)
                                                    {
                                                        read += bytes_transferred;
                                                        handler(ec, bytes_transferred);
                                                    },
                                                    std::move(handler)));
            return result.get();
        }
    };
}
