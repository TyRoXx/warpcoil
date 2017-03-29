#pragma once

#include <vector>

namespace warpcoil
{
    template <class T, class... Elements>
    std::vector<T> make_vector(Elements &&... elements)
    {
        std::vector<T> result;
        result.reserve(sizeof...(Elements));
        (void)std::initializer_list<int>{
            (result.emplace_back(std::forward<Elements>(elements)), 0)...};
        return result;
    }
}
