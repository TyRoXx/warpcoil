#include "hello_interfaces.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <silicium/to_shared.hpp>
#include <silicium/make_destructor.hpp>
#include <iostream>
#include <future>

namespace warpcoil
{
    struct yield_context
    {
        std::shared_ptr<std::promise<void>> handled;
    };

    namespace detail
    {
        template <class T>
        struct coroutine_handler
        {
            explicit coroutine_handler(yield_context context)
                : context(std::move(context))
                , result(std::make_shared<std::promise<T>>())
            {
                assert(this->context.handled);
            }

            coroutine_handler(coroutine_handler const &) = default;
            coroutine_handler &operator=(coroutine_handler const &) = default;

            std::future<T> get_future()
            {
                return result->get_future();
            }

            void operator()(T value)
            {
                result->set_value(std::move(value));
                context.handled->get_future().get();
            }

            void operator()(boost::system::error_code ec, T value)
            {
                if (!!ec)
                {
                    result->set_exception(std::make_exception_ptr(boost::system::system_error(ec)));
                }
                else
                {
                    result->set_value(std::move(value));
                }
                context.handled->get_future().get();
            }

            void resume()
            {
                context.handled->set_value();
            }

            yield_context context;
            std::shared_ptr<std::promise<T>> result;
        };

        template <>
        struct coroutine_handler<void>
        {
            explicit coroutine_handler(yield_context context)
                : context(std::move(context))
                , result(std::make_shared<std::promise<void>>())
            {
                assert(this->context.handled);
            }

            coroutine_handler(coroutine_handler const &) = default;
            coroutine_handler &operator=(coroutine_handler const &) = default;

            std::future<void> get_future()
            {
                return result->get_future();
            }

            void operator()(boost::system::error_code ec)
            {
                if (!!ec)
                {
                    result->set_exception(std::make_exception_ptr(boost::system::system_error(ec)));
                }
                else
                {
                    result->set_value();
                }
                context.handled->get_future().get();
            }

            void resume()
            {
                context.handled->set_value();
            }

            yield_context context;
            std::shared_ptr<std::promise<void>> result;
        };
    }

    template <class Function>
    void spawn_coroutine(boost::asio::io_service &io, Function &&body)
    {
        auto state = std::make_shared<std::future<void>>();
        auto work = std::make_shared<boost::optional<boost::asio::io_service::work>>(io);
        *state = std::async(
            std::launch::async, [&io, state, body = std::forward<Function>(body), work ]() mutable
            {
                boost::asio::io_service::work const work_on_stack(io);
                *work = boost::none;
                auto state_on_stack = std::move(state);
                assert(!state);
                std::packaged_task<void(yield_context)> task([copyable_body = Si::to_shared(
                                                                  std::forward<Function>(body))](yield_context yield)
                                                             {
                                                                 return (*copyable_body)(yield);
                                                             });
                yield_context context{std::make_shared<std::promise<void>>()};
                task(context);
                io.post([ result = task.get_future().share(), state_on_stack = std::move(state_on_stack) ]() mutable
                        {
                            result.get();
                        });
                assert(!state_on_stack);
            });
    }
}

namespace boost
{
    namespace asio
    {
        template <class T>
        struct async_result<warpcoil::detail::coroutine_handler<T>>
        {
            typedef T type;

            explicit async_result(warpcoil::detail::coroutine_handler<T> &handler)
                : handler(handler)
            {
                *handler.context.handled = std::promise<void>();
            }

            type get()
            {
                auto on_exit = Si::make_destructor([this]
                                                   {
                                                       handler.resume();
                                                   });
                return handler.get_future().get();
            }

        private:
            warpcoil::detail::coroutine_handler<T> &handler;
        };

        template <typename ReturnType, class A2>
        struct handler_type<warpcoil::yield_context, ReturnType(A2)>
        {
            typedef warpcoil::detail::coroutine_handler<A2> type;
        };

        template <typename ReturnType, class A2>
        struct handler_type<warpcoil::yield_context, ReturnType(boost::system::error_code, A2)>
        {
            typedef warpcoil::detail::coroutine_handler<A2> type;
        };

        template <typename ReturnType>
        struct handler_type<warpcoil::yield_context, ReturnType(boost::system::error_code)>
        {
            typedef warpcoil::detail::coroutine_handler<void> type;
        };
    }
}

namespace server
{
    struct my_hello_service : async_hello_as_a_service
    {
        virtual void hello(std::string argument,
                           std::function<void(boost::system::error_code, std::string)> on_result) override
        {
            on_result({}, "Hello, " + argument + "!");
        }
    };
}

int main()
{
    using namespace boost::asio;
    io_service io;

    // server:
    ip::tcp::acceptor acceptor(io, ip::tcp::endpoint(ip::tcp::v6(), 0), true);
    acceptor.listen();
    warpcoil::spawn_coroutine(
        io, [&io, &acceptor](warpcoil::yield_context yield)
        {
            ip::tcp::socket accepted_socket(io);
            server::my_hello_service server_impl;
            acceptor.async_accept(accepted_socket, yield);
            async_hello_as_a_service_server<decltype(server_impl), ip::tcp::socket, ip::tcp::socket> server(
                server_impl, accepted_socket, accepted_socket);
            server.serve_one_request(yield);
        });
    // client:
    warpcoil::spawn_coroutine(io, [&io, &acceptor](warpcoil::yield_context yield)
                              {
                                  ip::tcp::socket connecting_socket(io);
                                  async_hello_as_a_service_client<ip::tcp::socket, ip::tcp::socket> client(
                                      connecting_socket, connecting_socket);
                                  connecting_socket.async_connect(
                                      ip::tcp::endpoint(ip::address_v4::loopback(), acceptor.local_endpoint().port()),
                                      yield);
                                  std::string name = "Alice";
                                  std::string result = client.hello(std::move(name), yield);
                                  std::cout << result << '\n';
                              });

    io.run();
}
