#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "mmo/core/entity.hpp"
#include "mmo/core/event.hpp"
#include "mmo/core/item.hpp"
#include "mmo/core/time.hpp"
#include "mmo/core/zone.hpp"

namespace mmo
{
    namespace world
    {
        struct World
        {
            core::entity::Table entities;
            core::zone::Table zones;
            core::event::Scheduler scheduler;

            // Domain events leave the scheduler through this observable boundary.
            std::vector<core::event::Event> event_outbox;

            // Invalid events and failed state transitions are retained for diagnosis/retry.
            std::vector<core::event::Event> rejected_events;

            auto spawn_entity(
                core::id::EntityId entity_id,
                const core::entity::Blueprint& blueprint,
                core::id::ZoneId zone_id,
                core::time::TimePoint now) -> bool
            {
                const bool is_player = blueprint.type == core::entity::Type::player;
                if (!zones.can_add_entity(zone_id, is_player) ||
                    !entities.spawn(entity_id, blueprint, zone_id, now))
                {
                    return false;
                }

                zones.add_entity(zone_id, is_player, now);
                refresh_zone_activity(zone_id);
                return true;
            }

            auto erase_entity(
                core::id::EntityId entity_id,
                core::time::TimePoint now) -> bool
            {
                const auto& entity_view = entities;
                const auto* record = entity_view.find(entity_id);
                if (record == nullptr)
                {
                    return false;
                }

                const auto zone_id = record->placement.zone_id();
                const bool is_player = record->identity.type == core::entity::Type::player;
                if (!zones.can_remove_entity(zone_id, is_player) ||
                    !entities.erase(entity_id))
                {
                    return false;
                }

                if (!zones.remove_entity(zone_id, is_player, now))
                {
                    return false;
                }

                refresh_zone_activity(zone_id);
                return true;
            }

            auto move_entity_to_zone(
                core::id::EntityId entity_id,
                core::id::ZoneId destination_zone_id,
                core::time::TimePoint now) -> bool
            {
                const auto& entity_view = entities;
                const auto* record = entity_view.find(entity_id);
                if (record == nullptr ||
                    destination_zone_id == core::id::invalid_zone_id)
                {
                    return false;
                }

                const auto source_zone_id = record->placement.zone_id();
                if (source_zone_id == destination_zone_id)
                {
                    return true;
                }

                const bool is_player = record->identity.type == core::entity::Type::player;
                if (!zones.can_remove_entity(source_zone_id, is_player) ||
                    !zones.can_add_entity(destination_zone_id, is_player) ||
                    !entities.move_to_zone(entity_id, destination_zone_id))
                {
                    return false;
                }

                if (!zones.remove_entity(source_zone_id, is_player, now))
                {
                    entities.move_to_zone(entity_id, source_zone_id);
                    return false;
                }

                zones.add_entity(destination_zone_id, is_player, now);
                refresh_zone_activity(source_zone_id);
                refresh_zone_activity(destination_zone_id);
                return true;
            }

            auto wake_zone(core::id::ZoneId zone_id) -> bool
            {
                if (zone_id == core::id::invalid_zone_id)
                {
                    return false;
                }

                zones.wake(zone_id);
                refresh_zone_activity(zone_id);
                return true;
            }

            auto sleep_zone(core::id::ZoneId zone_id) -> bool
            {
                if (zone_id == core::id::invalid_zone_id || !zones.sleep(zone_id))
                {
                    return false;
                }

                refresh_zone_activity(zone_id);
                return true;
            }

            [[nodiscard]] auto should_tick_full(core::id::ZoneId zone_id) const -> bool
            {
                return active_zone_ids_.count(zone_id) != 0;
            }

            [[nodiscard]] auto active_zone_ids() const noexcept
                -> const std::set<core::id::ZoneId>&
            {
                return active_zone_ids_;
            }

            auto mark_zone_tick(
                core::id::ZoneId zone_id,
                core::time::TickCount tick) -> void
            {
                if (should_tick_full(zone_id))
                {
                    zones.mark_tick(zone_id, tick);
                }
            }

            [[nodiscard]] auto has_consistent_spatial_state() const -> bool
            {
                if (!entities.has_consistent_indexes())
                {
                    return false;
                }

                struct Population
                {
                    std::uint64_t entities{ 0 };
                    std::uint64_t players{ 0 };
                };

                std::map<core::id::ZoneId, Population> populations;
                for (const auto entity_id : entities.list_ids())
                {
                    const auto* record = entities.find(entity_id);
                    if (record == nullptr)
                    {
                        return false;
                    }

                    auto& population = populations[record->placement.zone_id()];
                    ++population.entities;
                    if (record->identity.type == core::entity::Type::player)
                    {
                        ++population.players;
                    }
                }

                for (const auto zone_id : zones.list_ids())
                {
                    const auto* state = zones.get(zone_id);
                    if (state == nullptr)
                    {
                        return false;
                    }

                    const auto population_it = populations.find(zone_id);
                    const Population population = population_it == populations.end()
                        ? Population{}
                        : population_it->second;
                    if (state->entity_count != population.entities ||
                        state->player_count != population.players ||
                        should_tick_full(zone_id) != state->is_active())
                    {
                        return false;
                    }

                    populations.erase(zone_id);
                }

                for (const auto zone_id : active_zone_ids_)
                {
                    if (zones.get(zone_id) == nullptr)
                    {
                        return false;
                    }
                }

                return populations.empty();
            }

        private:
            std::set<core::id::ZoneId> active_zone_ids_;

            auto refresh_zone_activity(core::id::ZoneId zone_id) -> void
            {
                const auto* state = zones.get(zone_id);
                if (state != nullptr && state->is_active())
                {
                    active_zone_ids_.insert(zone_id);
                }
                else
                {
                    active_zone_ids_.erase(zone_id);
                }
            }
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
            const core::event::Event& scheduled_event) -> EventDispatchOutcome
        {
            if (!core::event::is_valid(scheduled_event))
            {
                world.rejected_events.push_back(scheduled_event);
                return EventDispatchOutcome::rejected;
            }

            switch (scheduled_event.type)
            {
                case core::event::Type::migration_completed:
                    if (world.move_entity_to_zone(
                            scheduled_event.entity_id,
                            scheduled_event.zone_id,
                            scheduled_event.due_at))
                    {
                        return EventDispatchOutcome::applied;
                    }
                    break;

                case core::event::Type::zone_wake:
                    if (world.wake_zone(scheduled_event.zone_id))
                    {
                        return EventDispatchOutcome::applied;
                    }
                    break;

                case core::event::Type::zone_sleep:
                    if (world.sleep_zone(scheduled_event.zone_id))
                    {
                        return EventDispatchOutcome::applied;
                    }
                    break;

                case core::event::Type::evolution_due:
                case core::event::Type::region_notice:
                default:
                    world.event_outbox.push_back(scheduled_event);
                    return EventDispatchOutcome::queued;
            }

            world.rejected_events.push_back(scheduled_event);
            return EventDispatchOutcome::rejected;
        }

        [[nodiscard]] inline auto dispatch_ready_events(
            World& world,
            core::time::TimePoint now) -> EventDispatchStats
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

        struct TickContext
        {
            core::time::TickCount tick_index{ 0 };
            core::time::TimePoint simulation_time{};
        };

        struct TickStats
        {
            std::uint64_t events_processed{ 0 };
            std::uint64_t events_applied{ 0 };
            std::uint64_t events_queued{ 0 };
            std::uint64_t events_rejected{ 0 };
            std::uint64_t active_zones{ 0 };
            std::uint64_t entities_considered{ 0 };
            std::uint64_t entities_skipped{ 0 };
            std::uint64_t load_recalculations{ 0 };
            std::uint64_t status_sweeps{ 0 };
            std::uint64_t status_changes{ 0 };
            std::uint64_t status_periodic_applications{ 0 };
        };

        inline constexpr std::uint64_t max_periodic_catch_up_applications_per_status = 4;

        enum class TickPhase : std::uint8_t
        {
            scheduled_events = 0,
            entity_maintenance,
            periodic_statuses,
            status_sweep
        };

        class TickObserver
        {
        public:
            virtual ~TickObserver() = default;
            virtual auto phase_started(TickPhase phase) -> void = 0;
            virtual auto phase_finished(TickPhase phase) -> void = 0;
        };

        [[nodiscard]] inline auto list_entity_ids_in_simulation_order(
            const World& world) -> std::vector<core::id::EntityId>
        {
            return world.entities.list_ids();
        }

        // Canonical active order is (ZoneId, EntityId); both indexes are ordered trees.
        [[nodiscard]] inline auto list_active_entity_ids_in_simulation_order(
            const World& world) -> std::vector<core::id::EntityId>
        {
            std::vector<core::id::EntityId> ids;
            for (const auto zone_id : world.active_zone_ids())
            {
                const auto zone_ids = world.entities.ids_in_zone(zone_id);
                ids.insert(ids.end(), zone_ids.begin(), zone_ids.end());
            }

            return ids;
        }

        namespace detail
        {
            inline auto notify_phase_started(TickObserver* observer, TickPhase phase) -> void
            {
                if (observer != nullptr)
                {
                    observer->phase_started(phase);
                }
            }

            inline auto notify_phase_finished(TickObserver* observer, TickPhase phase) -> void
            {
                if (observer != nullptr)
                {
                    observer->phase_finished(phase);
                }
            }
        }

        // Canonical tick order: events, entity maintenance, periodic statuses, expiration sweep.
        [[nodiscard]] inline auto step(
            World& world,
            const core::item::Catalog& items,
            const TickContext& context,
            TickObserver* observer = nullptr) -> TickStats
        {
            TickStats stats{};

            detail::notify_phase_started(observer, TickPhase::scheduled_events);
            const auto event_stats = dispatch_ready_events(world, context.simulation_time);
            stats.events_processed = event_stats.total();
            stats.events_applied = event_stats.applied;
            stats.events_queued = event_stats.queued;
            stats.events_rejected = event_stats.rejected;
            detail::notify_phase_finished(observer, TickPhase::scheduled_events);

            const auto ids = list_active_entity_ids_in_simulation_order(world);
            stats.active_zones = world.active_zone_ids().size();
            stats.entities_considered = ids.size();
            stats.entities_skipped = world.entities.size() - ids.size();

            detail::notify_phase_started(observer, TickPhase::entity_maintenance);
            for (const auto zone_id : world.active_zone_ids())
            {
                world.mark_zone_tick(zone_id, context.tick_index);
            }
            for (const auto entity_id : ids)
            {
                if (world.entities.sync_inventory_load_if_dirty(entity_id, items))
                {
                    ++stats.load_recalculations;
                }
            }
            detail::notify_phase_finished(observer, TickPhase::entity_maintenance);

            detail::notify_phase_started(observer, TickPhase::periodic_statuses);
            for (const auto entity_id : ids)
            {
                const auto periodic = world.entities.process_periodic_statuses(
                    entity_id,
                    context.simulation_time,
                    max_periodic_catch_up_applications_per_status);
                stats.status_periodic_applications += periodic.applications;
            }
            detail::notify_phase_finished(observer, TickPhase::periodic_statuses);

            detail::notify_phase_started(observer, TickPhase::status_sweep);
            for (const auto entity_id : ids)
            {
                ++stats.status_sweeps;
                if (world.entities.sweep_statuses(entity_id, context.simulation_time))
                {
                    ++stats.status_changes;
                }
            }
            detail::notify_phase_finished(observer, TickPhase::status_sweep);

            return stats;
        }
    }
}
