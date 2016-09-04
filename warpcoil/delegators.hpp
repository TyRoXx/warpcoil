#pragma once

#include <silicium/function.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/preprocessor/empty.hpp>
#include <boost/preprocessor/list/for_each_i.hpp>
#include <boost/preprocessor/variadic/to_list.hpp>

namespace warpcoil
{
#define DELEGATOR_INCLUDE <warpcoil/delegator_async_read_write_stream.hpp>
#include <silicium/delegator/generate.hpp>
}
