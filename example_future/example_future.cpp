#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <warpcoil/future.hpp>

namespace
{
    void delay(std::chrono::steady_clock::duration duration, boost::asio::io_service &io,
               boost::asio::yield_context yield)
    {
        boost::asio::steady_timer timeout(io);
        timeout.expires_from_now(duration);
        timeout.async_wait(yield);
    }

    void try_to_connect(boost::asio::ip::tcp::endpoint server, boost::asio::io_service &io,
                        boost::asio::yield_context yield)
    {
        boost::asio::ip::tcp::socket socket(io, boost::asio::ip::tcp::v4());
        boost::asio::steady_timer timeout(io);

        using warpcoil::future;

        future<boost::system::error_code> connected = socket.async_connect(server, warpcoil::use_future);

        timeout.expires_from_now(std::chrono::seconds(2));
        future<boost::system::error_code> timed_out = timeout.async_wait(warpcoil::use_future);

        warpcoil::all(timed_out.then([&](boost::system::error_code)
                                     {
                                         socket.cancel();
                                     }),
                      connected.then([&](boost::system::error_code ec)
                                     {
                                         timeout.cancel();
                                         if (!!ec)
                                         {
                                             std::cerr << server << " Failed with error " << ec << '\n';
                                         }
                                         else
                                         {
                                             std::cerr << server << " Succeeded\n";
                                         }
                                     }))
            .async_wait(yield);
    }
}

int main()
{
    boost::asio::io_service io;
#ifdef _MSC_VER
    {
        // WinSock initialization crashes with an access violation if done within a Boost coroutine, so we do it here:
        boost::asio::ip::tcp::socket dummy(io, boost::asio::ip::tcp::v4());
    }
#endif
    boost::asio::spawn(io, [&io](boost::asio::yield_context yield)
                       {
                           for (;;)
                           {
                               try_to_connect(boost::asio::ip::tcp::endpoint(
                                                  boost::asio::ip::address_v4::from_string("172.217.20.195"), 80),
                                              io, yield);
                               delay(std::chrono::seconds(1), io, yield);
                           }
                       });
    boost::asio::spawn(
        io, [&io](boost::asio::yield_context yield)
        {
            for (;;)
            {
                try_to_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::from_string("1.2.3.4"), 80),
                               io, yield);
            }
        });
    io.run();
}
