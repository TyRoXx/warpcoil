#pragma once

#include <silicium/variant.hpp>
#include <boost/asio/async_result.hpp>
#include <functional>

namespace warpcoil
{
    struct use_future_type
    {
    };

    constexpr use_future_type use_future;

    namespace detail
    {
        template <class Result>
        struct future_handler
        {
            std::shared_ptr<Si::variant<Si::unit, std::function<void(Result)>, Result>> state;

            explicit future_handler(use_future_type)
                : state(std::make_shared<
                        Si::variant<Si::unit, std::function<void(Result)>, Result>>())
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

    namespace detail
    {
        template <class T>
        struct apply_transform
        {
            template <class Transformation, class Input, class HandleResult>
            static void apply(Transformation &&transform, Input &&input,
                              HandleResult &&handle_result)
            {
                std::forward<HandleResult>(handle_result)(
                    std::forward<Transformation>(transform)(std::forward<Input>(input)));
            }

            template <class Transformation, class HandleResult>
            static void apply(Transformation &&transform, HandleResult &&handle_result)
            {
                std::forward<HandleResult>(handle_result)(
                    std::forward<Transformation>(transform)());
            }
        };

        template <>
        struct apply_transform<void>
        {
            template <class Transformation, class Input, class HandleResult>
            static void apply(Transformation &&transform, Input &&input,
                              HandleResult &&handle_result)
            {
                std::forward<Transformation>(transform)(std::forward<Input>(input));
                std::forward<HandleResult>(handle_result)();
            }

            template <class Transformation, class HandleResult>
            static void apply(Transformation &&transform, HandleResult &&handle_result)
            {
                std::forward<Transformation>(transform)();
                std::forward<HandleResult>(handle_result)();
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
            assert(_wait);
            using handler_type =
                typename boost::asio::handler_type<decltype(token), void(Result)>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            _wait(std::move(handler));
            return result.get();
        }

        template <class Transformation>
        auto then(Transformation &&transform)
        {
            using transformed_result = decltype(transform(std::declval<Result>()));
            auto wait = std::move(_wait);
            assert(!_wait);
            return future<transformed_result>(
                [ wait = std::move(wait), transform = std::forward<Transformation>(transform) ](
                    std::function<void(transformed_result)> on_result) mutable
                {
                    wait([
                        on_result = std::move(on_result),
                        transform = std::forward<Transformation>(transform)
                    ](Result intermediate) mutable
                         {
                             detail::apply_transform<transformed_result>::apply(
                                 std::forward<Transformation>(transform),
                                 std::forward<Result>(intermediate), std::move(on_result));
                         });
                });
        }

    private:
        std::function<void(std::function<void(Result)>)> _wait;
    };

    template <>
    struct future<void>
    {
        explicit future(std::function<void(std::function<void()>)> wait)
            : _wait(std::move(wait))
        {
        }

        template <class CompletionToken>
        auto async_wait(CompletionToken &&token)
        {
            assert(_wait);
            using handler_type = typename boost::asio::handler_type<decltype(token), void()>::type;
            handler_type handler(std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(handler);
            _wait(std::move(handler));
            return result.get();
        }

        template <class Transformation>
        auto then(Transformation &&transform)
        {
            using transformed_result = decltype(transform());
            auto wait = std::move(_wait);
            assert(!_wait);
            return future<transformed_result>(
                [ wait = std::move(wait), transform = std::forward<Transformation>(transform) ](
                    std::function<void(transformed_result)> on_result) mutable
                {
                    wait([
                        on_result = std::move(on_result),
                        transform = std::forward<Transformation>(transform)
                    ]() mutable
                         {
                             detail::apply_transform<transformed_result>::apply(
                                 std::forward<Transformation>(transform), std::move(on_result));
                         });
                });
        }

    private:
        std::function<void(std::function<void()>)> _wait;
    };

    template <class... Futures>
    auto when_all(Futures &&... futures)
    {
        return future<void>(std::function<void(std::function<void()>)>(std::bind<void>(
            [](auto &&on_result, auto &&... futures) mutable
            {
                static constexpr size_t future_count = sizeof...(futures);
                using expand_type = int[];
                auto callback = [ completed = std::make_shared<size_t>(0), on_result ]()
                {
                    ++*completed;
                    if (*completed == future_count)
                    {
                        on_result();
                    }
                };
                expand_type{(futures.async_wait(callback), 0)...};
            },
            std::placeholders::_1, std::forward<Futures>(futures)...)));
    }
}

namespace boost
{
    namespace asio
    {
        template <class Result>
        struct async_result<warpcoil::detail::future_handler<Result>>
        {
            using type = warpcoil::future<Result>;

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
                                                    Si::visit<void>(
                                                        *state,
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
