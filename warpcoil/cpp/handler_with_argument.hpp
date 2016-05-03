#pragma once

#include <boost/asio/handler_invoke_hook.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class Handler, class Argument>
        struct handler_with_argument
        {
            explicit handler_with_argument(Handler handler, Argument argument)
                : m_handler(std::move(handler))
                , m_argument(std::move(argument))
            {
            }

            template <class... Args>
            void operator()(Args &&... args)
            {
                m_handler(std::forward<Args>(args)..., m_argument);
            }

            template <class Function>
            friend void asio_handler_invoke(Function &&f, handler_with_argument *operation)
            {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(f, &operation->m_handler);
            }

        private:
            Handler m_handler;
            Argument m_argument;
        };

        template <class Handler, class Argument>
        auto make_handler_with_argument(Handler &&handler, Argument &&argument)
        {
            return handler_with_argument<typename std::decay<Handler>::type, typename std::decay<Argument>::type>(
                std::forward<Handler>(handler), std::forward<Argument>(argument));
        }
    }
}
