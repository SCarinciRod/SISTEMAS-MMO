#pragma once

#if __cplusplus < 201703L
#include <experimental/optional>
#include <experimental/string_view>

namespace std
{
    using experimental::make_optional;
    using experimental::nullopt;
    using experimental::nullopt_t;
    using experimental::optional;
    using experimental::string_view;

    template <typename T>
    constexpr const T& clamp(const T& value, const T& low, const T& high)
    {
        return value < low ? low : (high < value ? high : value);
    }
}
#endif
