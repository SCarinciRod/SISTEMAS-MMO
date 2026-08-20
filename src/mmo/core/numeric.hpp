#pragma once

#include <cstdint>
#include <initializer_list>
#include <limits>

namespace mmo
{
    namespace core
    {
        namespace numeric
        {
            [[nodiscard]] constexpr auto clamp_to_int32(std::int64_t value) noexcept -> std::int32_t
            {
                return value > std::numeric_limits<std::int32_t>::max()
                    ? std::numeric_limits<std::int32_t>::max()
                    : value < std::numeric_limits<std::int32_t>::min()
                        ? std::numeric_limits<std::int32_t>::min()
                        : static_cast<std::int32_t>(value);
            }

            [[nodiscard]] constexpr auto saturating_add(
                std::int32_t lhs,
                std::int32_t rhs) noexcept -> std::int32_t
            {
                return clamp_to_int32(
                    static_cast<std::int64_t>(lhs) + static_cast<std::int64_t>(rhs));
            }

            [[nodiscard]] inline auto saturating_add(
                std::initializer_list<std::int32_t> values) noexcept -> std::int32_t
            {
                auto result = std::int32_t{ 0 };

                for (const auto value : values)
                {
                    result = saturating_add(result, value);
                }

                return result;
            }

            [[nodiscard]] constexpr auto saturating_add(
                std::uint32_t lhs,
                std::uint32_t rhs) noexcept -> std::uint32_t
            {
                const auto sum = static_cast<std::uint64_t>(lhs) + rhs;
                return sum > std::numeric_limits<std::uint32_t>::max()
                    ? std::numeric_limits<std::uint32_t>::max()
                    : static_cast<std::uint32_t>(sum);
            }

            [[nodiscard]] constexpr auto multiply_divide(
                std::int32_t value,
                std::int32_t numerator,
                std::int32_t denominator) noexcept -> std::int32_t
            {
                if (denominator == 0)
                {
                    return 0;
                }

                const auto product =
                    static_cast<std::int64_t>(value) * static_cast<std::int64_t>(numerator);
                return clamp_to_int32(product / denominator);
            }

            [[nodiscard]] constexpr auto scale_non_negative(
                std::int32_t value,
                std::uint64_t numerator,
                std::uint64_t denominator) noexcept -> std::int32_t
            {
                if (value <= 0 || numerator == 0 || denominator == 0)
                {
                    return 0;
                }

                const auto unsigned_value = static_cast<std::uint64_t>(value);
                if (numerator > std::numeric_limits<std::uint64_t>::max() / unsigned_value)
                {
                    return std::numeric_limits<std::int32_t>::max();
                }

                const auto scaled = (unsigned_value * numerator) / denominator;
                return scaled > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())
                    ? std::numeric_limits<std::int32_t>::max()
                    : static_cast<std::int32_t>(scaled);
            }

            [[nodiscard]] constexpr auto scale_non_negative(
                std::uint32_t value,
                std::uint32_t numerator,
                std::uint32_t denominator) noexcept -> std::uint32_t
            {
                if (value == 0 || numerator == 0 || denominator == 0)
                {
                    return 0;
                }

                const auto scaled =
                    (static_cast<std::uint64_t>(value) * numerator) / denominator;
                return scaled > std::numeric_limits<std::uint32_t>::max()
                    ? std::numeric_limits<std::uint32_t>::max()
                    : static_cast<std::uint32_t>(scaled);
            }
        }
    }
}
