#pragma once

#include <boost/test/test_tools.hpp>

namespace boost
{
    namespace test_tools
    {
        namespace tt_detail
        {
            template <typename T>
            struct print_log_value<std::vector<T>>
            {
                void operator()(std::ostream &out, std::vector<T> const &v)
                {
                    out << '{';
                    bool first = true;
                    for (T const &e : v)
                    {
                        if (first)
                        {
                            first = false;
                        }
                        else
                        {
                            out << ", ";
                        }
                        out << static_cast<unsigned>(e);
                    }
                    out << '}';
                }
            };
        }
    }
}