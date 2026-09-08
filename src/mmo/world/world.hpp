#pragma once

#include <cstdint>
#include <exception>
#include <stdexcept>
#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <vector>

#include "mmo/core/entity.hpp"
#include "mmo/core/event.hpp"
#include "mmo/core/item.hpp"
#include "mmo/core/time.hpp"
#include "mmo/core/zone.hpp"
#include "mmo/world/command.hpp"
#include "mmo/world/domain_event.hpp"

namespace mmo
{
    namespace world
    {
        struct TickContext;
        struct TickStats;
        class TickObserver;
        class World;

        [[nodiscard]] auto step(
            World& world,
            const core::item::Catalog& items,
            const TickContext& context,
            const command::Batch& commands,
            TickObserver* observer) -> TickStats;

        class World
        {
        public:
            [[nodiscard]] auto is_faulted() const noexcept -> bool { return faulted_; }
            auto mark_faulted() noexcept -> void { faulted_ = true; }

            [[nodiscard]] auto find_entity(core::id::EntityId entity_id) const
                -> const core::entity::Record*
            {
                return entities_.find(entity_id);
            }

            [[nodiscard]] auto find_zone(core::id::ZoneId zone_id) const
                -> const core::zone::State*
            {
                return zones_.get(zone_id);
            }

            [[nodiscard]] auto list_entity_ids() const
                -> std::vector<core::id::EntityId>
            {
                return entities_.list_ids();
            }

            [[nodiscard]] auto entity_ids_in_zone(core::id::ZoneId zone_id) const
                -> std::vector<core::id::EntityId>
            {
                return entities_.ids_in_zone(zone_id);
            }

            [[nodiscard]] auto entity_count() const noexcept -> std::size_t
            {
                return entities_.size();
            }

            [[nodiscard]] auto pending_event_count() const noexcept -> std::size_t
            {
                return scheduler_.size();
            }

            [[nodiscard]] auto event_queue_empty() const noexcept -> bool
            {
                return scheduler_.empty();
            }

            [[nodiscard]] auto next_event_due() const
                -> std::optional<core::time::TimePoint>
            {
                return scheduler_.next_due();
            }

            [[nodiscard]] auto pending_events() const noexcept
                -> const std::vector<core::event::Event>&
            {
                return event_outbox_;
            }

            [[nodiscard]] auto pending_rejected_events() const noexcept
                -> const std::vector<core::event::Event>&
            {
                return rejected_events_;
            }

            [[nodiscard]] auto drain_events() -> std::vector<core::event::Event>
            {
                std::vector<core::event::Event> drained;
                drained.swap(event_outbox_);
                return drained;
            }

            [[nodiscard]] auto drain_rejected_events() -> std::vector<core::event::Event>
            {
                std::vector<core::event::Event> drained;
                drained.swap(rejected_events_);
                return drained;
            }

            [[nodiscard]] auto try_schedule_event(const core::event::Event& event) -> bool
            {
                return scheduler_.try_schedule(event);
            }

            auto apply_status(
                core::id::EntityId entity_id,
                const core::status::Instance& instance) -> bool
            {
                return entities_.apply_status(entity_id, instance);
            }

            auto adjust_health(
                core::id::EntityId entity_id,
                std::int32_t delta,
                core::time::TimePoint simulation_time) -> bool
            {
                return entities_.adjust_health(entity_id, delta, simulation_time);
            }

            auto adjust_mana(core::id::EntityId entity_id, std::int32_t delta) -> bool
            {
                return entities_.adjust_mana(entity_id, delta);
            }

            auto apply_damage(
                core::id::EntityId entity_id,
                const core::combat::DamageProfile& profile,
                core::time::TimePoint simulation_time)
                -> std::optional<core::combat::DamageResult>
            {
                return entities_.apply_damage(entity_id, profile, simulation_time);
            }

            auto apply_damage(
                core::id::EntityId entity_id,
                const core::combat::DamageProfile& profile,
                const core::combat::DefenseProfile& defense,
                core::time::TimePoint simulation_time)
                -> std::optional<core::combat::DamageResult>
            {
                return entities_.apply_damage(entity_id, profile, defense, simulation_time);
            }

            auto add_inventory_item(
                core::id::EntityId entity_id,
                const core::item::Catalog& catalog,
                core::item::Instance instance) -> core::inventory::AddResult
            {
                return entities_.add_inventory_item(entity_id, catalog, instance);
            }

            [[nodiscard]] auto validate_inventory(
                core::id::EntityId entity_id,
                const core::item::Catalog& catalog) const
                -> core::inventory::ValidationIssue
            {
                return entities_.validate_inventory(entity_id, catalog);
            }

            auto spawn_entity(
                core::id::EntityId entity_id,
                const core::entity::Blueprint& blueprint,
                core::id::ZoneId zone_id,
                core::time::TimePoint now) -> bool
            {
                const bool is_player = blueprint.type == core::entity::Type::player;
                if (!zones_.can_add_entity(zone_id, is_player) ||
                    !entities_.spawn(entity_id, blueprint, zone_id, now))
                {
                    return false;
                }

                zones_.add_entity(zone_id, is_player, now);
                refresh_zone_activity(zone_id);
                return true;
            }

            auto erase_entity(
                core::id::EntityId entity_id,
                core::time::TimePoint now) -> bool
            {
                const auto* record = entities_.find(entity_id);
                if (record == nullptr)
                {
                    return false;
                }

                const auto zone_id = record->placement.zone_id();
                const bool is_player = record->identity.type == core::entity::Type::player;
                if (!zones_.can_remove_entity(zone_id, is_player) ||
                    !entities_.erase(entity_id))
                {
                    return false;
                }

                if (!zones_.remove_entity(zone_id, is_player, now))
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
                const auto* record = entities_.find(entity_id);
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
                if (!zones_.can_remove_entity(source_zone_id, is_player) ||
                    !zones_.can_add_entity(destination_zone_id, is_player) ||
                    !entities_.move_to_zone(entity_id, destination_zone_id))
                {
                    return false;
                }

                if (!zones_.remove_entity(source_zone_id, is_player, now))
                {
                    entities_.move_to_zone(entity_id, source_zone_id);
                    return false;
                }

                zones_.add_entity(destination_zone_id, is_player, now);
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

                zones_.wake(zone_id);
                refresh_zone_activity(zone_id);
                return true;
            }

            auto sleep_zone(core::id::ZoneId zone_id) -> bool
            {
                if (zone_id == core::id::invalid_zone_id || !zones_.sleep(zone_id))
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

            [[nodiscard]] auto has_consistent_spatial_state() const -> bool
            {
                if (!entities_.has_consistent_indexes())
                {
                    return false;
                }

                struct Population
                {
                    std::uint64_t entities{ 0 };
                    std::uint64_t players{ 0 };
                };

                std::map<core::id::ZoneId, Population> populations;
                for (const auto entity_id : entities_.list_ids())
                {
                    const auto* record = entities_.find(entity_id);
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

                for (const auto zone_id : zones_.list_ids())
                {
                    const auto* state = zones_.get(zone_id);
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
                    if (zones_.get(zone_id) == nullptr)
                    {
                        return false;
                    }
                }

                return populations.empty();
            }

        private:
            bool faulted_{ false };
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

            core::entity::Table entities_;
            core::zone::Table zones_;
            core::event::Scheduler scheduler_;
            std::vector<core::event::Event> event_outbox_;
            std::vector<core::event::Event> rejected_events_;
            std::set<core::id::ZoneId> active_zone_ids_;

            friend auto step(
                World& world,
                const core::item::Catalog& items,
                const TickContext& context,
                const command::Batch& commands,
                TickObserver* observer) -> TickStats;

            auto mark_zone_tick(
                core::id::ZoneId zone_id,
                core::time::TickCount tick) -> void
            {
                if (should_tick_full(zone_id))
                {
                    zones_.mark_tick(zone_id, tick);
                }
            }

            auto refresh_zone_activity(core::id::ZoneId zone_id) -> void
            {
                const auto* state = zones_.get(zone_id);
                if (state != nullptr && state->is_active())
                {
                    active_zone_ids_.insert(zone_id);
                }
                else
                {
                    active_zone_ids_.erase(zone_id);
                }
            }

            [[nodiscard]] auto dispatch_event_internal(
                const core::event::Event& scheduled_event) -> EventDispatchOutcome
            {
                if (!core::event::is_valid(scheduled_event))
                {
                    rejected_events_.push_back(scheduled_event);
                    return EventDispatchOutcome::rejected;
                }

                switch (scheduled_event.type)
                {
                    case core::event::Type::migration_completed:
                        if (move_entity_to_zone(
                                scheduled_event.entity_id,
                                scheduled_event.zone_id,
                                scheduled_event.due_at))
                        {
                            return EventDispatchOutcome::applied;
                        }
                        break;

                    case core::event::Type::zone_wake:
                        if (wake_zone(scheduled_event.zone_id))
                        {
                            return EventDispatchOutcome::applied;
                        }
                        break;

                    case core::event::Type::zone_sleep:
                        if (sleep_zone(scheduled_event.zone_id))
                        {
                            return EventDispatchOutcome::applied;
                        }
                        break;

                    case core::event::Type::evolution_due:
                    case core::event::Type::region_notice:
                    default:
                        event_outbox_.push_back(scheduled_event);
                        return EventDispatchOutcome::queued;
                }

                rejected_events_.push_back(scheduled_event);
                return EventDispatchOutcome::rejected;
            }

            [[nodiscard]] auto dispatch_ready_events_internal(
                core::time::TimePoint now) -> EventDispatchStats
            {
                const auto ready_events = scheduler_.pop_ready(now);
                EventDispatchStats stats{};

                for (const auto& scheduled_event : ready_events)
                {
                    switch (dispatch_event_internal(scheduled_event))
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

            [[nodiscard]] auto execute_command_internal(
                const command::Envelope& envelope,
                const core::item::Catalog& items,
                core::time::TimePoint simulation_time) -> command::ExecutionResult
            {
                if (envelope.actor == core::id::invalid_entity_id)
                {
                    return { envelope.sequence, command::ExecutionRejection::invalid_actor };
                }

                const auto* actor = entities_.find(envelope.actor);
                if (actor == nullptr)
                {
                    return { envelope.sequence, command::ExecutionRejection::entity_missing };
                }

                return std::visit(
                    [this, &envelope, &items, actor, simulation_time](const auto& payload)
                        -> command::ExecutionResult
                    {
                        using PayloadType = std::decay_t<decltype(payload)>;
                        if constexpr (std::is_same_v<PayloadType, command::MoveToZone>)
                        {
                            if (payload.destination_zone_id == core::id::invalid_zone_id)
                            {
                                return {
                                    envelope.sequence,
                                    command::ExecutionRejection::invalid_destination
                                };
                            }

                            if (actor->placement.zone_id() == payload.destination_zone_id)
                            {
                                return {
                                    envelope.sequence,
                                    command::ExecutionRejection::already_in_destination
                                };
                            }

                            if (!move_entity_to_zone(
                                    envelope.actor,
                                    payload.destination_zone_id,
                                    simulation_time))
                            {
                                return {
                                    envelope.sequence,
                                    command::ExecutionRejection::transition_rejected
                                };
                            }

                            return { envelope.sequence, command::ExecutionRejection::none };
                        }
                        else if constexpr (std::is_same_v<PayloadType, command::AdjustHealth>)
                        {
                            if (payload.delta == 0)
                                return { envelope.sequence, command::ExecutionRejection::invalid_quantity };
                            return { envelope.sequence, adjust_health(envelope.actor, payload.delta, simulation_time)
                                ? command::ExecutionRejection::none : command::ExecutionRejection::transition_rejected };
                        }
                        else
                        {
                            if (payload.item_id == core::id::invalid_item_id || items.find(payload.template_id) == nullptr)
                                return { envelope.sequence, command::ExecutionRejection::invalid_item };
                            core::item::Instance instance{};
                            instance.item_id = payload.item_id;
                            instance.item_template_id = payload.template_id;
                            instance.acquired_at = simulation_time;
                            const auto result = add_inventory_item(envelope.actor, items, instance);
                            return { envelope.sequence, result.quantity_added == 1
                                ? command::ExecutionRejection::none : command::ExecutionRejection::transition_rejected };
                        }
                    },
                    envelope.payload);
            }
        };

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
            std::uint64_t commands_received{ 0 };
            std::uint64_t commands_processed{ 0 };
            std::uint64_t commands_accepted{ 0 };
            std::uint64_t commands_rejected{ 0 };
            std::vector<command::ExecutionResult> command_results{};
            std::vector<domain::Event> domain_events{};
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
            authoritative_commands,
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
            return world.list_entity_ids();
        }

        // Canonical active order is (ZoneId, EntityId); both indexes are ordered trees.
        [[nodiscard]] inline auto list_active_entity_ids_in_simulation_order(
            const World& world) -> std::vector<core::id::EntityId>
        {
            std::vector<core::id::EntityId> ids;
            for (const auto zone_id : world.active_zone_ids())
            {
                const auto zone_ids = world.entity_ids_in_zone(zone_id);
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

        // Canonical tick order: events, commands, maintenance, periodic statuses, expiration sweep.
        [[nodiscard]] inline auto step(
            World& world,
            const core::item::Catalog& items,
            const TickContext& context,
            const command::Batch& commands,
            TickObserver* observer = nullptr) -> TickStats
        {
            if (world.is_faulted()) throw std::logic_error("Cannot step a faulted World");
            struct FaultGuard
            {
                World& world;
                int exceptions{ std::uncaught_exceptions() };
                ~FaultGuard()
                {
                    if (std::uncaught_exceptions() > exceptions) world.mark_faulted();
                }
            } fault_guard{ world };
            TickStats stats{};

            detail::notify_phase_started(observer, TickPhase::scheduled_events);
            const auto event_stats = world.dispatch_ready_events_internal(context.simulation_time);
            stats.events_processed = event_stats.total();
            stats.events_applied = event_stats.applied;
            stats.events_queued = event_stats.queued;
            stats.events_rejected = event_stats.rejected;
            detail::notify_phase_finished(observer, TickPhase::scheduled_events);

            detail::notify_phase_started(observer, TickPhase::authoritative_commands);
            stats.commands_received = commands.size();
            stats.command_results.reserve(commands.size());
            for (const auto& envelope : commands)
            {
                const auto* before = world.find_entity(envelope.actor);
                const auto previous_zone = before ? before->placement.zone_id() : core::id::invalid_zone_id;
                const auto previous_health = before ? before->resources.health_current : 0;
                const auto result = commands.target_tick() != context.tick_index
                    ? command::ExecutionResult{ envelope.sequence, command::ExecutionRejection::invalid_tick }
                    : world.execute_command_internal(
                    envelope,
                    items,
                    context.simulation_time);
                stats.command_results.push_back(result);
                ++stats.commands_processed;
                if (result.accepted())
                {
                    ++stats.commands_accepted;
                    const auto* after = world.find_entity(envelope.actor);
                    if (std::holds_alternative<command::MoveToZone>(envelope.payload))
                        stats.domain_events.push_back({ context.tick_index, context.simulation_time, envelope.sequence,
                            domain::EntityMoved{ envelope.actor, previous_zone, after->placement.zone_id() } });
                    else if (std::holds_alternative<command::AdjustHealth>(envelope.payload))
                    {
                        if (previous_health != after->resources.health_current)
                            stats.domain_events.push_back({ context.tick_index, context.simulation_time, envelope.sequence,
                                domain::HealthAdjusted{ envelope.actor, previous_health, after->resources.health_current } });
                    }
                    else
                        stats.domain_events.push_back({ context.tick_index, context.simulation_time, envelope.sequence,
                            domain::InventoryItemAdded{ envelope.actor,
                                std::get<command::AddInventoryItem>(envelope.payload).template_id, 1 } });
                }
                else
                {
                    ++stats.commands_rejected;
                }
            }
            detail::notify_phase_finished(observer, TickPhase::authoritative_commands);

            const auto ids = list_active_entity_ids_in_simulation_order(world);
            stats.active_zones = world.active_zone_ids().size();
            stats.entities_considered = ids.size();
            stats.entities_skipped = world.entities_.size() - ids.size();

            detail::notify_phase_started(observer, TickPhase::entity_maintenance);
            for (const auto zone_id : world.active_zone_ids())
            {
                world.mark_zone_tick(zone_id, context.tick_index);
            }
            for (const auto entity_id : ids)
            {
                if (world.entities_.sync_inventory_load_if_dirty(entity_id, items))
                {
                    ++stats.load_recalculations;
                }
            }
            detail::notify_phase_finished(observer, TickPhase::entity_maintenance);

            detail::notify_phase_started(observer, TickPhase::periodic_statuses);
            for (const auto entity_id : ids)
            {
                const auto periodic = world.entities_.process_periodic_statuses(
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
                if (world.entities_.sweep_statuses(entity_id, context.simulation_time))
                {
                    ++stats.status_changes;
                }
            }
            detail::notify_phase_finished(observer, TickPhase::status_sweep);

            return stats;
        }
    }
}
