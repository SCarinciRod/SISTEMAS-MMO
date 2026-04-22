#pragma once

#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace error
        {
            enum class ErrorCode : std::uint16_t
            {
                none = 0,
                invalid_argument,
                not_found,
                already_exists,
                unauthorized,
                forbidden,
                timeout,
                disconnected,
                capacity_reached,
                unsupported,
                internal_error
            };

            inline constexpr bool succeeded(ErrorCode code) noexcept
            {
                return code == ErrorCode::none;
            }

            inline constexpr bool failed(ErrorCode code) noexcept
            {
                return !succeeded(code);
            }
        }
    }
}
