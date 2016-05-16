#pragma once

namespace warpcoil
{
    typedef std::string identifier;
    typedef std::uint64_t request_id;
    typedef std::uint8_t message_type_int;

    enum class message_type : message_type_int
    {
        request = 0,
        response = 1
    };
}
