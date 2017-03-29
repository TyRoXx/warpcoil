#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <warpcoil/then.hpp>

namespace
{
    namespace asio = boost::asio;

    void delay(std::chrono::steady_clock::duration duration, asio::io_service &io, asio::yield_context yield)
    {
        asio::steady_timer timeout(io);
        timeout.expires_from_now(duration);
        timeout.async_wait(yield);
    }

    void try_to_connect(asio::ip::tcp::endpoint server, asio::io_service &io, asio::yield_context yield)
    {
        using warpcoil::future;

        asio::ip::tcp::socket socket(io, asio::ip::tcp::v4());

        asio::steady_timer timeout(io);
        timeout.expires_from_now(std::chrono::seconds(2));

        warpcoil::when_all(timeout.async_wait(warpcoil::then([&](boost::system::error_code)
                                                             {
                                                                 socket.cancel();
                                                             })),
                           socket.async_connect(server, warpcoil::then([&](boost::system::error_code ec)
                                                                       {
                                                                           timeout.cancel();
                                                                           if (!!ec)
                                                                           {
                                                                               std::cerr << server
                                                                                         << " Failed with error " << ec
                                                                                         << '\n';
                                                                           }
                                                                           else
                                                                           {
                                                                               std::cerr << server << " Succeeded\n";
                                                                           }
                                                                       })))
            .async_wait(yield);
    }
}

int main()
{
    boost::asio::io_service io;
#ifdef _MSC_VER
    {
        // WinSock initialization crashes with an access violation if done within a Boost coroutine, so we do it here:
        asio::ip::tcp::socket dummy(io, asio::ip::tcp::v4());
    }
#endif
    boost::asio::spawn(io, [&io](asio::yield_context yield)
                       {
                           for (;;)
                           {
                               try_to_connect(
                                   asio::ip::tcp::endpoint(asio::ip::address_v4::from_string("172.217.20.195"), 80), io,
                                   yield);
                               delay(std::chrono::seconds(1), io, yield);
                           }
                       });
    boost::asio::spawn(io, [&io](asio::yield_context yield)
                       {
                           for (;;)
                           {
                               try_to_connect(asio::ip::tcp::endpoint(asio::ip::address_v4::from_string("1.2.3.4"), 80),
                                              io, yield);
                           }
                       });
    io.run();
}
