#pragma once

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <thread>

#include "mmo/core/config.hpp"
#include "mmo/core/item.hpp"
#include "mmo/core/logger.hpp"
#include "mmo/core/runtime.hpp"
#include "mmo/core/time.hpp"

namespace mmo
{
    namespace server
    {
        // Server loop contract: tick configuration and counters.
        struct LoopConfig
        {
            std::uint32_t tick_rate{ core::config::server_tick_rate };
            std::uint32_t max_ticks{ 0 };
            std::uint32_t max_catch_up_ticks{ 4 };
            bool sleep{ true };
        };

        struct LoopStats
        {
            std::uint64_t ticks{ 0 };
            std::uint64_t events_processed{ 0 };
            std::uint64_t events_applied{ 0 };
            std::uint64_t events_queued{ 0 };
            std::uint64_t events_rejected{ 0 };
            std::uint64_t entities_processed{ 0 };
            std::uint64_t load_recalculations{ 0 };
            std::uint64_t late_ticks{ 0 };
            std::uint64_t ticks_skipped{ 0 };
            std::uint64_t tick_work_total_ns{ 0 };
            std::uint64_t tick_work_max_ns{ 0 };
            std::uint64_t max_schedule_lag_ns{ 0 };
            std::uint64_t event_phase_total_ns{ 0 };
            std::uint64_t entity_phase_total_ns{ 0 };
            std::uint64_t status_phase_total_ns{ 0 };

            // Quantidade de vezes que o loop verificou/swept status.
            std::uint64_t status_sweeps{ 0 };

            // Quantidade de sweeps que realmente alteraram estado.
            std::uint64_t status_changes{ 0 };

            // Quantidade de aplicações periódicas de status, como poison/regen.
            std::uint64_t status_periodic_effects{ 0 };
        };

        // Keeps at most max_catch_up_ticks overdue simulation steps.
        [[nodiscard]] constexpr auto calculate_skipped_ticks(
            std::uint64_t simulation_tick,
            std::uint64_t wall_tick,
            std::uint32_t max_catch_up_ticks) noexcept -> std::uint64_t
        {
            if (wall_tick < simulation_tick)
            {
                return 0;
            }

            const auto overdue_distance = wall_tick - simulation_tick;
            const auto overdue_ticks = overdue_distance == std::numeric_limits<std::uint64_t>::max()
                ? overdue_distance
                : overdue_distance + 1;
            const auto catch_up_limit = std::max<std::uint64_t>(1, max_catch_up_ticks);
            return overdue_ticks > catch_up_limit
                ? overdue_ticks - catch_up_limit
                : 0;
        }

        // Consumes only due periodic status applications before sweeping statuses.
        [[nodiscard]] inline auto apply_periodic_status_effects(
            core::runtime::World& world,
            core::id::EntityId entity_id,
            core::time::TimePoint now) -> std::uint64_t
        {
            auto* record = world.entities.find(entity_id);
            if (record == nullptr)
            {
                return 0;
            }

            const auto totals = record->combat.status_effects.consume_periodic(now);

            auto remaining_health_delta = totals.health_delta;
            while (remaining_health_delta != 0)
            {
                record = world.entities.find(entity_id);
                if (record == nullptr)
                {
                    break;
                }

                std::int64_t bounded_delta{};
                if (remaining_health_delta > 0)
                {
                    bounded_delta = std::min<std::int64_t>(
                        remaining_health_delta,
                        static_cast<std::int64_t>(record->stats.current_derived.max_hp) -
                            record->resources.health_current);
                }
                else
                {
                    const auto available =
                        static_cast<std::int64_t>(record->resources.health_current) +
                        record->resources.shield_current;
                    const auto requested_damage = remaining_health_delta ==
                            std::numeric_limits<std::int64_t>::min()
                        ? std::numeric_limits<std::int64_t>::max()
                        : -remaining_health_delta;
                    const auto bounded_damage = std::min<std::int64_t>(
                        { requested_damage,
                          available,
                          std::numeric_limits<std::int32_t>::max() });
                    bounded_delta = -bounded_damage;
                }

                if (bounded_delta == 0)
                {
                    break;
                }

                world.entities.adjust_health(
                    entity_id,
                    static_cast<std::int32_t>(bounded_delta));
                remaining_health_delta -= bounded_delta;
            }

            record = world.entities.find(entity_id);
            if (record != nullptr && totals.mana_delta != 0)
            {
                const auto minimum_delta = -static_cast<std::int64_t>(
                    record->resources.mana_current);
                const auto maximum_delta = static_cast<std::int64_t>(
                    record->stats.current_derived.max_mana) - record->resources.mana_current;
                const auto bounded_delta = std::clamp(
                    totals.mana_delta,
                    minimum_delta,
                    maximum_delta);

                world.entities.adjust_mana(
                    entity_id,
                    static_cast<std::int32_t>(bounded_delta));
            }

            return totals.applications;
        }

        // Main tick loop: events, entity updates, and status processing per tick.
        [[nodiscard]] inline auto run_loop(
            core::runtime::World& world,
            const core::item::Catalog& catalog,
            const LoopConfig& config,
            core::log::Logger& logger) -> LoopStats
        {
            LoopStats stats{};

            const auto safe_tick_rate = core::time::normalize_tick_rate(config.tick_rate);
            const auto epoch = core::time::now();
            auto simulation_tick = std::uint64_t{ 0 };

            while (config.max_ticks == 0 || stats.ticks < config.max_ticks)
            {
                auto scheduled_at = core::time::tick_time(
                    epoch,
                    simulation_tick,
                    safe_tick_rate);

                if (config.sleep)
                {
                    auto current_time = core::time::now();

                    if (current_time < scheduled_at)
                    {
                        std::this_thread::sleep_until(scheduled_at);
                        current_time = core::time::now();
                    }

                    const auto wall_tick = core::time::duration_to_ticks(
                        current_time - epoch,
                        safe_tick_rate);
                    const auto skipped_ticks = calculate_skipped_ticks(
                        simulation_tick,
                        wall_tick,
                        config.max_catch_up_ticks);

                    if (skipped_ticks > 0)
                    {
                        simulation_tick += skipped_ticks;
                        stats.ticks_skipped += skipped_ticks;
                        scheduled_at = core::time::tick_time(
                            epoch,
                            simulation_tick,
                            safe_tick_rate);
                    }

                    if (wall_tick > simulation_tick)
                    {
                        ++stats.late_ticks;
                        const auto lag = std::chrono::duration_cast<core::time::Nanoseconds>(
                            current_time - scheduled_at).count();
                        if (lag > 0)
                        {
                            stats.max_schedule_lag_ns = std::max(
                                stats.max_schedule_lag_ns,
                                static_cast<std::uint64_t>(lag));
                        }
                    }
                }

                const auto tick_started_at = core::time::now();

                try
                {
                    const auto event_phase_started_at = core::time::now();
                    const auto event_stats = core::runtime::dispatch_ready_events(world, scheduled_at);
                    stats.events_processed += event_stats.total();
                    stats.events_applied += event_stats.applied;
                    stats.events_queued += event_stats.queued;
                    stats.events_rejected += event_stats.rejected;
                    stats.event_phase_total_ns += static_cast<std::uint64_t>(
                        std::max<core::time::Nanoseconds::rep>(
                            0,
                            std::chrono::duration_cast<core::time::Nanoseconds>(
                                core::time::now() - event_phase_started_at).count()));

                    const auto ids = world.entities.list_ids();
                    stats.entities_processed += ids.size();

                    const auto entity_phase_started_at = core::time::now();
                    for (const auto entity_id : ids)
                    {
                        auto* record = world.entities.find(entity_id);

                        if (record == nullptr)
                        {
                            continue;
                        }

                        if (world.entities.sync_inventory_load_if_dirty(entity_id, catalog))
                        {
                            ++stats.load_recalculations;
                            record = world.entities.find(entity_id);
                            if (record == nullptr)
                            {
                                continue;
                            }
                        }

                        world.zones.mark_tick(record->placement.zone_id(), simulation_tick);
                    }
                    stats.entity_phase_total_ns += static_cast<std::uint64_t>(
                        std::max<core::time::Nanoseconds::rep>(
                            0,
                            std::chrono::duration_cast<core::time::Nanoseconds>(
                                core::time::now() - entity_phase_started_at).count()));

                    const auto status_phase_started_at = core::time::now();
                    for (const auto entity_id : ids)
                    {
                        stats.status_periodic_effects +=
                            apply_periodic_status_effects(world, entity_id, scheduled_at);

                        ++stats.status_sweeps;

                        if (world.entities.sweep_statuses(entity_id, scheduled_at))
                        {
                            ++stats.status_changes;
                        }
                    }
                    stats.status_phase_total_ns += static_cast<std::uint64_t>(
                        std::max<core::time::Nanoseconds::rep>(
                            0,
                            std::chrono::duration_cast<core::time::Nanoseconds>(
                                core::time::now() - status_phase_started_at).count()));
                }
                catch (const std::exception& ex)
                {
                    core::log::log_exception(logger, "server.loop.tick", ex);
                }
                catch (...)
                {
                    core::log::log_exception(logger, "server.loop.tick");
                }

                const auto work_nanoseconds = std::chrono::duration_cast<core::time::Nanoseconds>(
                    core::time::now() - tick_started_at).count();
                const auto safe_work_nanoseconds = work_nanoseconds > 0
                    ? static_cast<std::uint64_t>(work_nanoseconds)
                    : 0;
                stats.tick_work_total_ns += safe_work_nanoseconds;
                stats.tick_work_max_ns = std::max(
                    stats.tick_work_max_ns,
                    safe_work_nanoseconds);

                ++stats.ticks;
                ++simulation_tick;
            }

            return stats;
        }
    }
}
