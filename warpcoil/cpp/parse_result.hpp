#pragma once

#include <boost/system/error_code.hpp>
#include <silicium/variant.hpp>

namespace warpcoil
{
    namespace cpp
    {
        struct need_more_input
        {
        };

        struct invalid_input
        {
        };

        struct invalid_input_error_category : boost::system::error_category
        {
            virtual const char *name() const BOOST_SYSTEM_NOEXCEPT override
            {
                return "invalid_input";
            }

            virtual std::string message(int) const override
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
        using parse_result = Si::variant<T, need_more_input, invalid_input>;
    }
}
