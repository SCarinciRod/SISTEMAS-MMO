#pragma once

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <thread>

#include "mmo/core/config.hpp"
#include "mmo/core/item.hpp"
#include "mmo/core/logger.hpp"
#include "mmo/core/time.hpp"
#include "mmo/world/world.hpp"

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
            std::uint64_t active_zone_ticks{ 0 };
            std::uint64_t entities_processed{ 0 };
            std::uint64_t entities_skipped{ 0 };
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

        inline auto aggregate_tick_stats(
            LoopStats& loop_stats,
            const mmo::world::TickStats& tick_stats) -> void
        {
            loop_stats.events_processed += tick_stats.events_processed;
            loop_stats.events_applied += tick_stats.events_applied;
            loop_stats.events_queued += tick_stats.events_queued;
            loop_stats.events_rejected += tick_stats.events_rejected;
            loop_stats.active_zone_ticks += tick_stats.active_zones;
            loop_stats.entities_processed += tick_stats.entities_considered;
            loop_stats.entities_skipped += tick_stats.entities_skipped;
            loop_stats.load_recalculations += tick_stats.load_recalculations;
            loop_stats.status_sweeps += tick_stats.status_sweeps;
            loop_stats.status_changes += tick_stats.status_changes;
            loop_stats.status_periodic_effects += tick_stats.status_periodic_applications;
        }

        namespace detail
        {
            class LoopTickObserver final : public mmo::world::TickObserver
            {
            public:
                explicit LoopTickObserver(LoopStats& stats)
                    : stats_(stats)
                {
                }

                auto phase_started(mmo::world::TickPhase) -> void override
                {
                    phase_started_at_ = core::time::now();
                }

                auto phase_finished(mmo::world::TickPhase phase) -> void override
                {
                    const auto elapsed = std::chrono::duration_cast<core::time::Nanoseconds>(
                        core::time::now() - phase_started_at_).count();
                    const auto safe_elapsed = static_cast<std::uint64_t>(
                        std::max<core::time::Nanoseconds::rep>(0, elapsed));

                    switch (phase)
                    {
                        case mmo::world::TickPhase::scheduled_events:
                            stats_.event_phase_total_ns += safe_elapsed;
                            break;
                        case mmo::world::TickPhase::entity_maintenance:
                            stats_.entity_phase_total_ns += safe_elapsed;
                            break;
                        case mmo::world::TickPhase::periodic_statuses:
                        case mmo::world::TickPhase::status_sweep:
                            stats_.status_phase_total_ns += safe_elapsed;
                            break;
                    }
                }

            private:
                LoopStats& stats_;
                core::time::TimePoint phase_started_at_{};
            };
        }

        // Main tick loop: events, entity updates, and status processing per tick.
        [[nodiscard]] inline auto run_loop(
            mmo::world::World& world,
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
                    detail::LoopTickObserver phase_observer{ stats };
                    const auto tick_stats = mmo::world::step(
                        world,
                        catalog,
                        mmo::world::TickContext{ simulation_tick, scheduled_at },
                        &phase_observer);
                    aggregate_tick_stats(stats, tick_stats);
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
