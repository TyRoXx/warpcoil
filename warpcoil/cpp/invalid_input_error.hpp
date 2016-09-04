#pragma once

#include <boost/system/error_code.hpp>

namespace warpcoil
{
    namespace cpp
    {
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
    }
}
