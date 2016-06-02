#include "memory.hpp"
#include <new>
#include <atomic>

namespace warpcoil
{
    namespace
    {
        std::atomic<std::uint64_t> allocation_counter;
    }

    std::uint64_t number_of_allocations()
    {
        return allocation_counter.load();
    }
}

void *operator new(std::size_t count)
{
    ++warpcoil::allocation_counter;
    return std::malloc(count);
}

void *operator new[](std::size_t count)
{
    ++warpcoil::allocation_counter;
    return std::malloc(count);
}

void operator delete(void *ptr)
{
    std::free(ptr);
}

void operator delete[](void *ptr)
{
    std::free(ptr);
}
