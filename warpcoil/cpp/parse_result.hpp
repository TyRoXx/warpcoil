#pragma once

#include <boost/system/error_code.hpp>
#include <silicium/variant.hpp>
#include <silicium/optional.hpp>

namespace warpcoil
{
    namespace cpp
    {
        enum class input_consumption
        {
            consumed,
            does_not_consume
        };

        template <class T>
        struct parse_complete
        {
            T result;
            input_consumption input;
        };

        struct need_more_input
        {
        };

        struct invalid_input
        {
        };

        struct invalid_input_error_category : boost::system::error_category
        {
            const char *name() const BOOST_SYSTEM_NOEXCEPT override
            {
                return "invalid_input";
            }

            std::string message(int) const override
            {
                return "invalid input";
            }
        };

        inline boost::system::error_code make_invalid_input_error()
        {
            static invalid_input_error_category const category;
            return boost::system::error_code(1, category);
        }

        template <class T>
        using parse_result = Si::variant<parse_complete<T>, need_more_input, invalid_input>;

        template <class Parser>
        Si::optional<typename Parser::result_type> check_for_immediate_completion(Parser const &)
        {
            return Si::none;
        }
    }
}
