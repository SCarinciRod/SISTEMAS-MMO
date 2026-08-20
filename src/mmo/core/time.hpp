#pragma once

#include "compat.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>

namespace mmo
{
    namespace core
    {
        namespace time
        {
            using Clock = std::chrono::steady_clock;
            using TimePoint = Clock::time_point;
            using Duration = Clock::duration;
            using Nanoseconds = std::chrono::nanoseconds;
            using Milliseconds = std::chrono::milliseconds;
            using Seconds = std::chrono::seconds;
            using TickCount = std::uint64_t;

            constexpr std::uint32_t default_tick_rate = 20;
            constexpr Milliseconds default_tick_duration{1000 / default_tick_rate};
            constexpr std::uint32_t maximum_tick_rate = 1'000'000'000;

            [[nodiscard]] constexpr auto normalize_tick_rate(std::uint32_t tick_rate) noexcept -> std::uint32_t
            {
                return std::clamp(tick_rate, 1u, maximum_tick_rate);
            }

            // Uses the absolute tick index so fractional nanoseconds do not drift per tick.
            [[nodiscard]] constexpr auto tick_offset(
                TickCount tick,
                std::uint32_t tick_rate) noexcept -> Nanoseconds
            {
                const auto safe_tick_rate = normalize_tick_rate(tick_rate);
                const auto whole_seconds = tick / safe_tick_rate;
                const auto remaining_ticks = tick % safe_tick_rate;
                const auto maximum_seconds = static_cast<std::uint64_t>(
                    std::numeric_limits<Nanoseconds::rep>::max() / 1'000'000'000);

                if (whole_seconds > maximum_seconds)
                {
                    return Nanoseconds::max();
                }

                const auto whole_nanoseconds = whole_seconds * 1'000'000'000;
                const auto fractional_nanoseconds =
                    (remaining_ticks * 1'000'000'000) / safe_tick_rate;
                const auto maximum = static_cast<std::uint64_t>(
                    std::numeric_limits<Nanoseconds::rep>::max());

                if (fractional_nanoseconds > maximum - whole_nanoseconds)
                {
                    return Nanoseconds::max();
                }

                return Nanoseconds{ static_cast<Nanoseconds::rep>(
                    whole_nanoseconds + fractional_nanoseconds) };
            }

            [[nodiscard]] inline auto tick_time(
                TimePoint epoch,
                TickCount tick,
                std::uint32_t tick_rate) noexcept -> TimePoint
            {
                return epoch + std::chrono::duration_cast<Duration>(
                    tick_offset(tick, tick_rate));
            }

            [[nodiscard]] inline auto duration_to_ticks(
                Duration duration,
                std::uint32_t tick_rate) noexcept -> TickCount
            {
                const auto nanoseconds = std::chrono::duration_cast<Nanoseconds>(duration).count();
                if (nanoseconds <= 0)
                {
                    return 0;
                }

                const auto safe_tick_rate = normalize_tick_rate(tick_rate);
                const auto unsigned_nanoseconds = static_cast<std::uint64_t>(nanoseconds);
                const auto whole_seconds = unsigned_nanoseconds / 1'000'000'000;
                const auto remaining_nanoseconds = unsigned_nanoseconds % 1'000'000'000;
                return (whole_seconds * safe_tick_rate) +
                    ((remaining_nanoseconds * safe_tick_rate) / 1'000'000'000);
            }

            [[nodiscard]] inline auto now() noexcept -> TimePoint
            {
                return Clock::now();
            }

            [[nodiscard]] inline auto ticks_to_milliseconds(TickCount ticks) noexcept -> Milliseconds
            {
                return std::chrono::duration_cast<Milliseconds>(
                    tick_offset(ticks, default_tick_rate));
            }

            [[nodiscard]] inline auto milliseconds_to_ticks(Milliseconds milliseconds) noexcept -> TickCount
            {
                return duration_to_ticks(milliseconds, default_tick_rate);
            }
        }
    }
}
