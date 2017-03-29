#pragma once

#include <boost/asio/handler_invoke_hook.hpp>

namespace warpcoil
{
    namespace cpp
    {
        template <class Function, class ActualHandler>
        struct wrapped_handler
        {
            explicit wrapped_handler(Function wrapper, ActualHandler handler)
                : m_wrapper(std::move(wrapper))
                , m_handler(std::move(handler))
            {
            }

            template <class... Args>
            void operator()(Args &&... args)
            {
                m_wrapper(std::forward<Args>(args)..., m_handler);
            }

            template <class F>
            friend void asio_handler_invoke(F &&f, wrapped_handler *operation)
            {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(f, &operation->m_handler);
            }

        private:
            Function m_wrapper;
            ActualHandler m_handler;
        };

        template <class Function, class ActualHandler>
        auto wrap_handler(Function &&wrapper, ActualHandler &&handler)
        {
            return wrapped_handler<typename std::decay<Function>::type,
                                   typename std::decay<ActualHandler>::type>(
                std::forward<Function>(wrapper), std::forward<ActualHandler>(handler));
        }
    }
}
