#include <boost/asio/async_result.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/buffer.hpp>
#include <silicium/exchange.hpp>

namespace warpcoil
{
    struct in_process_pipe
    {
        boost::asio::io_service &dispatcher;
        boost::asio::const_buffer writing;
        boost::asio::mutable_buffer reading;
        std::function<void(boost::system::error_code, std::size_t)> write_callback;
        std::function<void(boost::system::error_code, std::size_t)> read_callback;

        explicit in_process_pipe(boost::asio::io_service &dispatcher)
            : dispatcher(dispatcher)
        {
        }

        template <class ConstBufferSequence, class CompletionToken>
        auto async_write_some(ConstBufferSequence const &buffers, CompletionToken &&token)
        {
            assert(!write_callback);
            assert(!boost::empty(buffers));
            using handler_type =
                typename boost::asio::handler_type<decltype(token), void(boost::system::error_code, std::size_t)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            using boost::begin;
            writing = *begin(buffers);
            write_callback = std::move(handler);
            pump();
            return result.get();
        }

        template <class MutableBufferSequence, class CompletionToken>
        auto async_read_some(MutableBufferSequence const &buffers, CompletionToken &&token)
        {
            assert(!read_callback);
            assert(!boost::empty(buffers));
            using handler_type =
                typename boost::asio::handler_type<decltype(token), void(boost::system::error_code, std::size_t)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            using boost::begin;
            reading = *begin(buffers);
            read_callback = std::move(handler);
            pump();
            return result.get();
        }

    private:
        void pump()
        {
            if (write_callback && read_callback)
            {
                std::size_t const copied = boost::asio::buffer_copy(reading, writing);
                dispatcher.post([
                    write_callback = Si::exchange(write_callback, nullptr),
                    read_callback = Si::exchange(read_callback, nullptr),
                    copied
                ]()
                                {
                                    write_callback({}, copied);
                                    read_callback({}, copied);
                                });
            }
        }
    };
}
