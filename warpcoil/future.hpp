#pragma once

#include <silicium/variant.hpp>
#include <boost/asio/async_result.hpp>

namespace warpcoil
{
    constexpr struct use_future_type
    {
    } use_future;

    namespace detail
    {
        template <class Result>
        struct future_handler
        {
            std::shared_ptr<Si::variant<Si::unit, std::function<void(Result)>, Result>> state;

            explicit future_handler(use_future_type)
                : state(std::make_shared<Si::variant<Si::unit, std::function<void(Result)>, Result>>())
            {
            }

            void operator()(Result result)
            {
                Si::visit<void>(*state,
                                [this, &result](Si::unit)
                                {
                                    *state = std::move(result);
                                },
                                [&result](std::function<void(Result)> &handler)
                                {
                                    handler(std::move(result));
                                },
                                [](Result &)
                                {
                                    SILICIUM_UNREACHABLE();
                                });
            }
        };
    }

    template <class Result>
    struct future
    {
        explicit future(std::function<void(std::function<void(Result)>)> wait)
            : _wait(std::move(wait))
        {
        }

        template <class CompletionToken>
        auto async_wait(CompletionToken &&token)
        {
            using handler_type = typename boost::asio::handler_type<decltype(token), void(Result)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            _wait(std::move(handler));
            return result.get();
        }

    private:
        std::function<void(std::function<void(Result)>)> _wait;
    };
}

namespace boost
{
    namespace asio
    {
        template <class Result>
        struct async_result<warpcoil::detail::future_handler<Result>>
        {
            explicit async_result(warpcoil::detail::future_handler<Result> const &handler)
                : _handler(handler)
            {
                assert(_handler.state);
            }

            warpcoil::future<Result> get()
            {
                assert(_handler.state);
                return warpcoil::future<Result>([state = std::move(_handler.state)](
                    std::function<void(Result)> handle_result)
                                                {
                                                    Si::visit<void>(*state,
                                                                    [&state, &handle_result](Si::unit)
                                                                    {
                                                                        *state = std::move(handle_result);
                                                                    },
                                                                    [](std::function<void(Result)> &)
                                                                    {
                                                                        SILICIUM_UNREACHABLE();
                                                                    },
                                                                    [&handle_result](Result &result)
                                                                    {
                                                                        handle_result(std::move(result));
                                                                    });
                                                });
            }

        private:
            warpcoil::detail::future_handler<Result> _handler;
        };

        template <class Result>
        struct handler_type<warpcoil::use_future_type, void(Result)>
        {
            using type = warpcoil::detail::future_handler<Result>;
        };
    }
}
