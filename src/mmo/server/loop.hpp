#pragma once

#include <algorithm>
#include <cstdint>
#include <exception>
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
            bool sleep{ true };
        };

        struct LoopStats
        {
            std::uint64_t ticks{ 0 };
            std::uint64_t events_processed{ 0 };
            std::uint64_t entities_processed{ 0 };

            // Quantidade de vezes que o loop verificou/swept status.
            std::uint64_t status_sweeps{ 0 };

            // Quantidade de sweeps que realmente alteraram estado.
            std::uint64_t status_changes{ 0 };

            // Quantidade de aplicações periódicas de status, como poison/regen.
            std::uint64_t status_periodic_effects{ 0 };
        };

        // Applies periodic status deltas (regen, poison) before sweeping statuses.
        inline auto apply_periodic_status_effects(
            core::runtime::World& world,
            core::id::EntityId entity_id) -> bool
        {
            auto* record = world.entities.find(entity_id);
            if (record == nullptr)
            {
                return false;
            }

            const auto totals = record->combat.status_effects.aggregate();
            const auto& modifiers = totals.modifiers;

            bool applied = false;

            if (modifiers.health_delta_per_tick != 0)
            {
                world.entities.adjust_health(entity_id, modifiers.health_delta_per_tick);
                applied = true;
            }

            if (modifiers.mana_delta_per_tick != 0)
            {
                world.entities.adjust_mana(entity_id, modifiers.mana_delta_per_tick);
                applied = true;
            }

            return applied;
        }

        // Main tick loop: events, entity updates, and status processing per tick.
        [[nodiscard]] inline auto run_loop(
            core::runtime::World& world,
            const core::item::Catalog& catalog,
            const LoopConfig& config,
            core::log::Logger& logger) -> LoopStats
        {
            LoopStats stats{};

            const auto safe_tick_rate = std::max<std::uint32_t>(1, config.tick_rate);

            const auto tick_duration = core::time::Milliseconds{
                static_cast<core::time::Milliseconds::rep>(1000 / safe_tick_rate)
            };

            auto next_tick = core::time::now();

            while (config.max_ticks == 0 || stats.ticks < config.max_ticks)
            {
                if (config.sleep)
                {
                    const auto current_time = core::time::now();

                    if (current_time < next_tick)
                    {
                        std::this_thread::sleep_for(next_tick - current_time);
                    }
                }

                const auto now = core::time::now();

                try
                {
                    const auto ready_events = core::runtime::collect_ready_events(world, now);
                    stats.events_processed += ready_events.size();

                    const auto ids = world.entities.list_ids();
                    stats.entities_processed += ids.size();

                    for (const auto entity_id : ids)
                    {
                        auto* record = world.entities.find(entity_id);

                        if (record == nullptr)
                        {
                            continue;
                        }

                        core::entity::sync_load(*record, catalog);

                        world.zones.mark_tick(record->placement.zone_id, stats.ticks);

                        if (apply_periodic_status_effects(world, entity_id))
                        {
                            ++stats.status_periodic_effects;
                        }

                        ++stats.status_sweeps;

                        if (world.entities.sweep_statuses(entity_id, now))
                        {
                            ++stats.status_changes;
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    core::log::log_exception(logger, "server.loop.tick", ex);
                }
                catch (...)
                {
                    core::log::log_exception(logger, "server.loop.tick");
                }

                ++stats.ticks;
                next_tick += tick_duration;
            }

            return stats;
        }
    }
}