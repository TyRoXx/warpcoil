#include <benchmark/benchmark.h>
#include <boost/system/error_code.hpp>
#include "benchmark_interfaces.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <silicium/error_or.hpp>
#include <test/checkpoint.hpp>

namespace
{
    struct my_hello_service : async_benchmark_service_a
    {
        void evaluate(std::tuple<std::uint64_t, std::uint64_t> argument,
                      std::function<void(Si::error_or<std::uint64_t>)> on_result) override
        {
            on_result(std::get<0>(argument) * std::get<1>(argument));
        }
    };

    void RequestAndResponseOverLocalSocket(benchmark::State &state)
    {
        using namespace boost::asio;
        io_service io;

        ip::tcp::acceptor acceptor(io, ip::tcp::endpoint(ip::tcp::v6(), 0), true);
        acceptor.listen();
        ip::tcp::socket accepted_socket(io);
        my_hello_service server_impl;
        warpcoil::cpp::message_splitter<ip::tcp::socket> server_splitter(accepted_socket);
        warpcoil::cpp::buffered_writer<ip::tcp::socket> server_writer(accepted_socket);
        async_benchmark_service_a_server<decltype(server_impl), ip::tcp::socket, ip::tcp::socket>
            server(server_impl, server_splitter, server_writer);
        acceptor.async_accept(accepted_socket, [](boost::system::error_code ec)
                              {
                                  Si::throw_if_error(ec);
                              });

        ip::tcp::socket client_socket(io);
        warpcoil::cpp::message_splitter<decltype(client_socket)> client_splitter(client_socket);
        warpcoil::cpp::buffered_writer<ip::tcp::socket> client_writer(client_socket);
        async_benchmark_service_a_client<ip::tcp::socket, ip::tcp::socket> client(client_writer,
                                                                                  client_splitter);
        client_socket.async_connect(
            ip::tcp::endpoint(ip::address_v4::loopback(), acceptor.local_endpoint().port()),
            [](boost::system::error_code ec)
            {
                Si::throw_if_error(ec);
            });
        io.run();

        while (state.KeepRunning())
        {
            warpcoil::checkpoint client_side;
            client.evaluate(std::make_tuple(34, 45),
                            [&client_side](Si::error_or<std::uint64_t> result)
                            {
                                client_side.enter();
                                if (result.get() != (34u * 45u))
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
        }
    }
    BENCHMARK(RequestAndResponseOverLocalSocket);
}
