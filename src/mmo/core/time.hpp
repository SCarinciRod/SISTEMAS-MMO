#pragma once

#include "compat.hpp"

#include <chrono>
#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace time
        {
            using Clock = std::chrono::steady_clock;
            using TimePoint = Clock::time_point;
            using Duration = Clock::duration;
            using Milliseconds = std::chrono::milliseconds;
            using Seconds = std::chrono::seconds;
            using TickCount = std::uint64_t;

            constexpr std::uint32_t default_tick_rate = 20;
            constexpr Milliseconds default_tick_duration{1000 / default_tick_rate};

            [[nodiscard]] inline auto now() noexcept -> TimePoint
            {
                return Clock::now();
            }

            [[nodiscard]] inline auto ticks_to_milliseconds(TickCount ticks) noexcept -> Milliseconds
            {
                return Milliseconds{static_cast<Milliseconds::rep>(ticks * default_tick_duration.count())};
            }

            [[nodiscard]] inline auto milliseconds_to_ticks(Milliseconds milliseconds) noexcept -> TickCount
            {
                return static_cast<TickCount>(milliseconds.count() / default_tick_duration.count());
            }
        }
    }
}
