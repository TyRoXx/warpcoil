#include "in_process_pipe.hpp"
#include "byte_counter.hpp"
#include <benchmark/benchmark.h>
#include "benchmark_interfaces.hpp"
#include <silicium/error_or.hpp>
#include <test/checkpoint.hpp>
#include <boost/asio/io_service.hpp>
#include <queue>

namespace
{
    struct my_hello_service_a : async_benchmark_service_a
    {
        void evaluate(std::tuple<std::uint64_t, std::uint64_t> argument,
                      std::function<void(boost::system::error_code, std::uint64_t)> on_result) override
        {
            on_result({}, std::get<0>(argument) * std::get<1>(argument));
        }
    };

    struct my_hello_service_b : async_benchmark_service_b
    {
        void evaluate(std::string argument,
                      std::function<void(boost::system::error_code, std::uint64_t)> on_result) override
        {
            on_result({}, argument.size());
        }
    };

    template <class ClientServerProvider, class LoopBody>
    void BenchmarkWithInProcessPipe(benchmark::State &state, ClientServerProvider const &provider, LoopBody const &body)
    {
        boost::asio::io_service io;
        warpcoil::byte_counter<warpcoil::in_process_pipe> client_to_server(io);
        warpcoil::byte_counter<warpcoil::in_process_pipe> server_to_client(io);

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
            [](warpcoil::byte_counter<warpcoil::in_process_pipe> &client_to_server,
               warpcoil::byte_counter<warpcoil::in_process_pipe> &server_to_client, auto &&user)
            {
                my_hello_service_a server_impl;
                warpcoil::cpp::message_splitter<decltype(client_to_server)> server_splitter(client_to_server);
                warpcoil::cpp::buffered_writer<decltype(server_to_client)> server_writer(server_to_client);
                async_benchmark_service_a_server<decltype(server_impl), decltype(client_to_server),
                                                 decltype(server_to_client)> server(server_impl, server_splitter,
                                                                                    server_writer);

                warpcoil::cpp::message_splitter<decltype(server_to_client)> client_splitter(server_to_client);
                warpcoil::cpp::buffered_writer<decltype(client_to_server)> client_writer(client_to_server);
                async_benchmark_service_a_client<decltype(server_to_client), decltype(client_to_server)> client(
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

    void MediumRequestAndResponseOverInProcessPipe(benchmark::State &state)
    {
        std::string const message_content(state.range_x(), 'a');
        BenchmarkWithInProcessPipe(
            state,
            [](warpcoil::byte_counter<warpcoil::in_process_pipe> &client_to_server,
               warpcoil::byte_counter<warpcoil::in_process_pipe> &server_to_client, auto &&user)
            {
                my_hello_service_b server_impl;
                warpcoil::cpp::message_splitter<decltype(client_to_server)> server_splitter(client_to_server);
                warpcoil::cpp::buffered_writer<decltype(server_to_client)> server_writer(server_to_client);
                async_benchmark_service_b_server<decltype(server_impl), decltype(client_to_server),
                                                 decltype(server_to_client)> server(server_impl, server_splitter,
                                                                                    server_writer);

                warpcoil::cpp::message_splitter<decltype(server_to_client)> client_splitter(server_to_client);
                warpcoil::cpp::buffered_writer<decltype(client_to_server)> client_writer(client_to_server);
                async_benchmark_service_b_client<decltype(server_to_client), decltype(client_to_server)> client(
                    client_writer, client_splitter);

                user(client, server);
            },
            [&message_content](boost::asio::io_service &io, auto &client, auto &server)
            {
                warpcoil::checkpoint client_side;
                client.evaluate(message_content,
                                [&client_side, &message_content](boost::system::error_code ec, std::uint64_t result)
                                {
                                    client_side.enter();
                                    Si::throw_if_error(ec);
                                    if (result != message_content.size())
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
    BENCHMARK(MediumRequestAndResponseOverInProcessPipe)->Range(1, 1 << 20);
}
