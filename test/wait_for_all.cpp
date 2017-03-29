#include "impl_test_interface.hpp"
#include "checkpoint.hpp"
#include <boost/mpl/back.hpp>

namespace warpcoil
{
    namespace
    {
        namespace detail
        {
            template <class Handler, class TotalResult>
            struct wait_for_all_state
            {
                Handler handler;
                TotalResult total_result;
                std::size_t completed = 0;
                bool errored = false;

                template <class CompletionToken>
                wait_for_all_state(CompletionToken &&token)
                    : handler(std::forward<CompletionToken>(token))
                {
                }
            };

            template <std::size_t Index, std::size_t MethodCount, class Method, class State>
            void start_method(Method &&method, std::shared_ptr<State> const &state)
            {
                method.start(
                    [state](Si::error_or<typename std::decay<Method>::type::result> element)
                    {
                        if (!element.is_error())
                        {
                            std::get<Index>(state->total_result) = std::move(element.get());
                            ++state->completed;
                            if (state->completed == MethodCount)
                            {
                                state->handler(std::move(state->total_result));
                            }
                        }
                        else if (!Si::exchange(state->errored, true))
                        {
                            state->handler(element.error());
                        }
                    });
            }

            template <class State, class... Methods, std::size_t... Indices>
            void start_methods(std::shared_ptr<State> state,
                               std::integer_sequence<std::size_t, Indices...>,
                               Methods &&... methods)
            {
                Si::unit dummy[] = {
                    (start_method<Indices, sizeof...(Methods)>(methods, state), Si::unit())...};
                Si::ignore_unused_variable_warning(dummy);
            }

            template <class Callback>
            struct result_of_callback;

            template <class Result>
            struct result_of_callback<
                std::function<void(Si::error_or<Result, boost::system::error_code>)>>
            {
                using type = Result;
            };

            template <class Result, class Start>
            struct method_holder
            {
                using result = Result;
                Start start;
            };

            template <typename F, typename Tuple, size_t... S>
            decltype(auto) apply_tuple_impl(F &&fn, Tuple &&t, std::index_sequence<S...>)
            {
                return std::forward<F>(fn)(std::get<S>(std::forward<Tuple>(t))...);
            }

            template <typename F, typename Tuple>
            decltype(auto) apply_from_tuple(F &&fn, Tuple &&t)
            {
                std::size_t constexpr tSize =
                    std::tuple_size<typename std::remove_reference<Tuple>::type>::value;
                return apply_tuple_impl(std::forward<F>(fn), std::forward<Tuple>(t),
                                        std::make_index_sequence<tSize>());
            }
        }

        template <class... Methods, class CompletionToken>
        auto wait_for_all(CompletionToken &&token, Methods &&... methods)
        {
            BOOST_STATIC_ASSERT(sizeof...(Methods) >= 1);
            using total_result = std::tuple<typename std::decay<Methods>::type::result...>;
            using handler_type =
                typename boost::asio::handler_type<decltype(token), void(boost::system::error_code,
                                                                         total_result)>::type;
            auto state = std::make_shared<detail::wait_for_all_state<handler_type, total_result>>(
                std::forward<CompletionToken>(token));
            boost::asio::async_result<handler_type> result(state->handler);
            start_methods(std::move(state),
                          std::make_integer_sequence<std::size_t, sizeof...(Methods)>(),
                          std::forward<Methods>(methods)...);
            return result.get();
        }

        template <class Interface, class Parameter0, class... Parameters, class... Arguments>
        auto method(Interface &callee, void (Interface::*method_ptr)(Parameter0, Parameters...),
                    Arguments &&... arguments)
        {
            auto start = [
                &callee,
                method_ptr,
                arguments = std::make_tuple(std::forward<Arguments>(arguments)...)
            ](auto &&callback) mutable
            {
                using callback_type = decltype(callback);
                detail::apply_from_tuple(
                    [&callback, &callee, method_ptr](auto &&... arguments)
                    {
                        (callee.*method_ptr)(std::forward<decltype(arguments)>(arguments)...,
                                             std::forward<callback_type>(callback));
                    },
                    std::move(arguments));
            };
            using std_function_callback =
                typename boost::mpl::back<boost::mpl::vector<Parameters...>>::type;
            using result = typename detail::result_of_callback<std_function_callback>::type;
            return detail::method_holder<result, decltype(start)>{std::move(start)};
        }
    }
}

BOOST_AUTO_TEST_CASE(wait_for_all_1)
{
    warpcoil::impl_test_interface server;
    warpcoil::checkpoint got_result;
    got_result.enable();
    warpcoil::wait_for_all(
        [&got_result](Si::error_or<std::tuple<std::string>> result)
        {
            got_result.enter();
            BOOST_CHECK_EQUAL("Hello123", std::get<0>(result.get()));
        },
        warpcoil::method(server, &warpcoil::impl_test_interface::utf8, "Hello"));
}

BOOST_AUTO_TEST_CASE(wait_for_all_2)
{
    warpcoil::impl_test_interface server;
    warpcoil::checkpoint got_result;
    got_result.enable();
    warpcoil::wait_for_all(
        [&got_result](Si::error_or<std::tuple<std::string, std::uint16_t>> result)
        {
            got_result.enter();
            BOOST_CHECK_EQUAL("Hello123", std::get<0>(result.get()));
            BOOST_CHECK_EQUAL(123u, std::get<1>(result.get()));
        },
        warpcoil::method(server, &warpcoil::impl_test_interface::utf8, "Hello"),
        warpcoil::method(server, &warpcoil::impl_test_interface::atypical_int,
                         static_cast<boost::uint16_t>(123)));
}
