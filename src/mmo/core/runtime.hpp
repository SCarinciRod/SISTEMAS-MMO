#pragma once

#include <vector>

#include "entity.hpp"
#include "event.hpp"
#include "zone.hpp"

namespace mmo
{
    namespace core
    {
        namespace runtime
        {
            struct World
            {
                entity::Table entities;
                zone::Table zones;
                event::Scheduler scheduler;
            };

            [[nodiscard]] inline auto collect_ready_events(World& world, time::TimePoint now) -> std::vector<event::Event>
            {
                return world.scheduler.pop_ready(now);
            }
        }
    }
}
