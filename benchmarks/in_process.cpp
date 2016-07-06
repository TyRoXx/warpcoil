#include <benchmark/benchmark.h>
#include "benchmark_interfaces.hpp"
#include <silicium/error_or.hpp>
#include <test/checkpoint.hpp>
#include <boost/asio/io_service.hpp>
#include <queue>

namespace
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

    struct my_hello_service : async_benchmark_service
    {
        void evaluate(std::tuple<std::uint64_t, std::uint64_t> argument,
                      std::function<void(boost::system::error_code, std::uint64_t)> on_result) override
        {
            on_result({}, std::get<0>(argument) * std::get<1>(argument));
        }
    };

    template <class ClientServerProvider, class LoopBody>
    void BenchmarkWithInProcessPipe(benchmark::State &state, ClientServerProvider const &provider, LoopBody const &body)
    {
        boost::asio::io_service io;
        byte_counter<in_process_pipe> client_to_server(io);
        byte_counter<in_process_pipe> server_to_client(io);

        provider(client_to_server, server_to_client, [&io, &state, &body](auto &client, auto &server)
                 {
                     while (state.KeepRunning())
                     {
                         body(io, client, server);
                     }
                 });

        std::uint64_t const total_transferred = client_to_server.read + client_to_server.written;
        if (total_transferred > (std::numeric_limits<std::size_t>::max)())
        {
            boost::throw_exception(std::logic_error("Transferred bytes overflowed"));
        }
        state.SetBytesProcessed(static_cast<size_t>(total_transferred));
    }

    void TinyRequestAndResponseOverInProcessPipe(benchmark::State &state)
    {
        BenchmarkWithInProcessPipe(
            state,
            [](byte_counter<in_process_pipe> &client_to_server, byte_counter<in_process_pipe> &server_to_client,
               auto &&user)
            {
                my_hello_service server_impl;
                warpcoil::cpp::message_splitter<decltype(client_to_server)> server_splitter(client_to_server);
                warpcoil::cpp::buffered_writer<decltype(server_to_client)> server_writer(server_to_client);
                async_benchmark_service_server<decltype(server_impl), decltype(client_to_server),
                                               decltype(server_to_client)> server(server_impl, server_splitter,
                                                                                  server_writer);

                warpcoil::cpp::message_splitter<decltype(server_to_client)> client_splitter(server_to_client);
                warpcoil::cpp::buffered_writer<decltype(client_to_server)> client_writer(client_to_server);
                async_benchmark_service_client<decltype(server_to_client), decltype(client_to_server)> client(
                    client_writer, client_splitter);

                user(client, server);
            },
            [](boost::asio::io_service &io, auto &client, auto &server)
            {
                warpcoil::checkpoint client_side;
                client.evaluate(std::make_tuple(34, 45),
                                [&client_side](boost::system::error_code ec, std::uint64_t result)
                                {
                                    client_side.enter();
                                    Si::throw_if_error(ec);
                                    if (result != (34u * 45u))
                                    {
                                        boost::throw_exception(std::logic_error("wrong result"));
                                    }
                                });
                warpcoil::checkpoint server_side;
                server.serve_one_request([&server_side](boost::system::error_code ec)
                                         {
                                             server_side.enter();
                                             Si::throw_if_error(ec);
                                         });
                server_side.enable();
                client_side.enable();
                io.reset();
                io.run();
            });
    }
    BENCHMARK(TinyRequestAndResponseOverInProcessPipe);
}
