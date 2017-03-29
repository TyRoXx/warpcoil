#pragma once

#include <warpcoil/future.hpp>

namespace warpcoil
{
    namespace detail
    {
        template <class Action>
        struct then_type
        {
            Action action;
        };
    }

    template <class Action>
    auto then(Action &&action)
    {
        return detail::then_type<typename std::decay<Action>::type>{std::forward<Action>(action)};
    }

    namespace detail
    {
        template <class Action, class ActionInput>
        struct then_handler
        {
            Action action;
            std::shared_ptr<std::function<void()>> finished;

            explicit then_handler(then_type<Action> then)
                : action(std::forward<Action>(then.action))
                , finished(std::make_shared<std::function<void()>>())
            {
            }

            void operator()(ActionInput result)
            {
                action(std::forward<ActionInput>(result));
                std::move (*finished)();
            }
        };
    }
}

namespace boost
{
    namespace asio
    {
        template <class Action, class ActionInput>
        struct async_result<warpcoil::detail::then_handler<Action, ActionInput>>
        {
            using type = warpcoil::future<void>;

            std::shared_ptr<std::function<void()>> finished;

            explicit async_result(warpcoil::detail::then_handler<Action, ActionInput> const &handler)
                : finished(handler.finished)
            {
            }

            warpcoil::future<void> get()
            {
                assert(finished);
                return warpcoil::future<void>([finished = std::move(finished)](std::function<void()> on_result)
                                              {
                                                  assert(on_result);
                                                  assert(!*finished);
                                                  *finished = std::move(on_result);
                                              });
            }
        };

        template <class Action, class Result>
        struct handler_type<warpcoil::detail::then_type<Action>, void(Result)>
        {
            using type = warpcoil::detail::then_handler<Action, Result>;
        };
    }
}
