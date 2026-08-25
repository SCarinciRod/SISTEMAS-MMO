#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "mmo/core/item.hpp"
#include "mmo/core/runtime.hpp"
#include "mmo/core/time.hpp"

namespace mmo
{
    namespace world
    {
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
            std::uint64_t entities_considered{ 0 };
            std::uint64_t load_recalculations{ 0 };
            std::uint64_t status_sweeps{ 0 };
            std::uint64_t status_changes{ 0 };
            std::uint64_t status_periodic_applications{ 0 };
        };

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

        // Hash tables remain lookup structures. Simulation traversal is canonical by EntityId.
        [[nodiscard]] inline auto list_entity_ids_in_simulation_order(
            const core::runtime::World& runtime_world) -> std::vector<core::id::EntityId>
        {
            auto ids = runtime_world.entities.list_ids();
            std::sort(ids.begin(), ids.end());
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
            core::runtime::World& runtime_world,
            const core::item::Catalog& items,
            const TickContext& context,
            TickObserver* observer = nullptr) -> TickStats
        {
            TickStats stats{};

            detail::notify_phase_started(observer, TickPhase::scheduled_events);
            const auto event_stats = core::runtime::dispatch_ready_events(
                runtime_world,
                context.simulation_time);
            stats.events_processed = event_stats.total();
            stats.events_applied = event_stats.applied;
            stats.events_queued = event_stats.queued;
            stats.events_rejected = event_stats.rejected;
            detail::notify_phase_finished(observer, TickPhase::scheduled_events);

            const auto ids = list_entity_ids_in_simulation_order(runtime_world);
            stats.entities_considered = ids.size();

            detail::notify_phase_started(observer, TickPhase::entity_maintenance);
            for (const auto entity_id : ids)
            {
                if (runtime_world.entities.sync_inventory_load_if_dirty(entity_id, items))
                {
                    ++stats.load_recalculations;
                }

                const auto& entity_view = runtime_world.entities;
                const auto* record = entity_view.find(entity_id);
                if (record != nullptr)
                {
                    runtime_world.zones.mark_tick(
                        record->placement.zone_id(),
                        context.tick_index);
                }
            }
            detail::notify_phase_finished(observer, TickPhase::entity_maintenance);

            detail::notify_phase_started(observer, TickPhase::periodic_statuses);
            for (const auto entity_id : ids)
            {
                const auto periodic = runtime_world.entities.process_periodic_statuses(
                    entity_id,
                    context.simulation_time);
                stats.status_periodic_applications += periodic.applications;
            }
            detail::notify_phase_finished(observer, TickPhase::periodic_statuses);

            detail::notify_phase_started(observer, TickPhase::status_sweep);
            for (const auto entity_id : ids)
            {
                ++stats.status_sweeps;
                if (runtime_world.entities.sweep_statuses(
                        entity_id,
                        context.simulation_time))
                {
                    ++stats.status_changes;
                }
            }
            detail::notify_phase_finished(observer, TickPhase::status_sweep);

            return stats;
        }
    }
}
