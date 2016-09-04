#pragma once

#include <silicium/sink/append.hpp>

namespace warpcoil
{
    template <class CharSink>
    struct comma_separator
    {
        explicit comma_separator(CharSink out)
            : m_out(out)
            , m_first(true)
        {
        }

        void add_element()
        {
            if (m_first)
            {
                m_first = false;
                return;
            }
            Si::append(m_out, ", ");
        }

    private:
        CharSink m_out;
        bool m_first;
    };

    template <class CharSink>
    auto make_comma_separator(CharSink out)
    {
        return comma_separator<CharSink>(out);
    }
}
