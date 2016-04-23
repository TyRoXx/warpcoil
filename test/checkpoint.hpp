#pragma once

#include <boost/test/unit_test.hpp>
#include <boost/noncopyable.hpp>
#include <silicium/config.hpp>

namespace warpcoil
{
    struct checkpoint : private boost::noncopyable
    {
        checkpoint()
            : m_state(state::created)
        {
        }

        ~checkpoint()
        {
            BOOST_CHECK_EQUAL(state::crossed, m_state);
        }

        void enable()
        {
            BOOST_REQUIRE_EQUAL(state::created, m_state);
            m_state = state::enabled;
        }

        void enter()
        {
            BOOST_REQUIRE_EQUAL(state::enabled, m_state);
            m_state = state::crossed;
        }

        void require_crossed()
        {
            BOOST_REQUIRE_EQUAL(state::crossed, m_state);
        }

    private:
        enum class state
        {
            created,
            enabled,
            crossed
        };

        friend std::ostream &operator<<(std::ostream &out, state s)
        {
            switch (s)
            {
            case state::created:
                return out << "created";
            case state::enabled:
                return out << "enabled";
            case state::crossed:
                return out << "crossed";
            }
            SILICIUM_UNREACHABLE();
        }

        state m_state;
    };
}
