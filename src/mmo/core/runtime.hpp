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

                // Domain events leave the scheduler through this observable boundary.
                std::vector<event::Event> event_outbox;

                // Invalid events and failed state transitions are retained for diagnosis/retry.
                std::vector<event::Event> rejected_events;
            };

            enum class EventDispatchOutcome : std::uint8_t
            {
                applied,
                queued,
                rejected
            };

            struct EventDispatchStats
            {
                std::size_t applied{ 0 };
                std::size_t queued{ 0 };
                std::size_t rejected{ 0 };

                [[nodiscard]] auto total() const noexcept -> std::size_t
                {
                    return applied + queued + rejected;
                }
            };

            [[nodiscard]] inline auto dispatch_event(
                World& world,
                const event::Event& scheduled_event) -> EventDispatchOutcome
            {
                if (!event::is_valid(scheduled_event))
                {
                    world.rejected_events.push_back(scheduled_event);
                    return EventDispatchOutcome::rejected;
                }

                switch (scheduled_event.type)
                {
                    case event::Type::migration_completed:
                        if (!world.entities.move_to_zone(
                                scheduled_event.entity_id,
                                scheduled_event.zone_id))
                        {
                            world.rejected_events.push_back(scheduled_event);
                            return EventDispatchOutcome::rejected;
                        }

                        return EventDispatchOutcome::applied;

                    case event::Type::zone_wake:
                        world.zones.set_active(scheduled_event.zone_id, true);
                        return EventDispatchOutcome::applied;

                    case event::Type::zone_sleep:
                        world.zones.set_active(scheduled_event.zone_id, false);
                        return EventDispatchOutcome::applied;

                    case event::Type::evolution_due:
                    case event::Type::region_notice:
                    default:
                        world.event_outbox.push_back(scheduled_event);
                        return EventDispatchOutcome::queued;
                }
            }

            [[nodiscard]] inline auto dispatch_ready_events(
                World& world,
                time::TimePoint now) -> EventDispatchStats
            {
                const auto ready_events = world.scheduler.pop_ready(now);
                EventDispatchStats stats{};

                for (const auto& scheduled_event : ready_events)
                {
                    switch (dispatch_event(world, scheduled_event))
                    {
                        case EventDispatchOutcome::applied:
                            ++stats.applied;
                            break;
                        case EventDispatchOutcome::queued:
                            ++stats.queued;
                            break;
                        case EventDispatchOutcome::rejected:
                            ++stats.rejected;
                            break;
                    }
                }

                return stats;
            }
        }
    }
}
