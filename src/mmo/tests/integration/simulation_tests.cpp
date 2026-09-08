#include "mmo/tests/support/test.hpp"
#include "mmo/world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v<
    decltype(std::declval<mmo::core::entity::Table&>().find(
        std::declval<mmo::core::id::EntityId>())),
    const mmo::core::entity::Record*>);

static_assert(std::is_same_v<
    decltype(std::declval<mmo::world::World&>().find_entity(
        std::declval<mmo::core::id::EntityId>())),
    const mmo::core::entity::Record*>);

namespace
{
    namespace test = mmo::tests::support;

    constexpr auto epoch() -> mmo::core::time::TimePoint
    {
        return mmo::core::time::TimePoint{};
    }

    auto make_blueprint(
        mmo::core::entity::Type type = mmo::core::entity::Type::player)
        -> mmo::core::entity::Blueprint
    {
        mmo::core::entity::Blueprint blueprint{};
        blueprint.template_id = static_cast<mmo::core::id::EntityTemplateId>(1);
        blueprint.species_id = static_cast<mmo::core::id::SpeciesId>(1);
        blueprint.type = type;
        blueprint.display_name = "Simulation Entity";
        blueprint.base_primary = mmo::core::stat::Primary{ 12, 12, 8, 6, 5, 7, 4 };
        blueprint.level = 1;
        return blueprint;
    }

    auto spawn_entity(
        mmo::world::World& world,
        mmo::core::id::EntityId entity_id,
        mmo::core::id::ZoneId zone_id = static_cast<mmo::core::id::ZoneId>(1),
        mmo::core::entity::Type type = mmo::core::entity::Type::player) -> void
    {
        test::require(
            world.spawn_entity(entity_id, make_blueprint(type), zone_id, epoch()),
            "simulation entity should spawn");
    }

    auto apply_poison(
        mmo::world::World& world,
        mmo::core::id::EntityId entity_id) -> void
    {
        const auto poison = mmo::core::status::make_instance(
            mmo::core::status::make_poison_definition(),
            epoch(),
            1,
            entity_id);
        test::require(
            world.apply_status(entity_id, poison),
            "simulation poison should apply");
    }

    auto accumulate_stats(
        mmo::world::TickStats& totals,
        const mmo::world::TickStats& tick) -> void
    {
        totals.events_processed += tick.events_processed;
        totals.events_applied += tick.events_applied;
        totals.events_queued += tick.events_queued;
        totals.events_rejected += tick.events_rejected;
        totals.commands_received += tick.commands_received;
        totals.commands_processed += tick.commands_processed;
        totals.commands_accepted += tick.commands_accepted;
        totals.commands_rejected += tick.commands_rejected;
        totals.command_results.insert(
            totals.command_results.end(),
            tick.command_results.begin(),
            tick.command_results.end());
        totals.active_zones += tick.active_zones;
        totals.entities_considered += tick.entities_considered;
        totals.entities_skipped += tick.entities_skipped;
        totals.load_recalculations += tick.load_recalculations;
        totals.status_sweeps += tick.status_sweeps;
        totals.status_changes += tick.status_changes;
        totals.status_periodic_applications += tick.status_periodic_applications;
    }

    auto require_equal_stats(
        const mmo::world::TickStats& expected,
        const mmo::world::TickStats& actual,
        std::string_view label) -> void
    {
        test::require_equal(expected.events_processed, actual.events_processed, std::string(label).append(" events processed"));
        test::require_equal(expected.events_applied, actual.events_applied, std::string(label).append(" events applied"));
        test::require_equal(expected.events_queued, actual.events_queued, std::string(label).append(" events queued"));
        test::require_equal(expected.events_rejected, actual.events_rejected, std::string(label).append(" events rejected"));
        test::require_equal(
            expected.commands_received,
            actual.commands_received,
            std::string(label).append(" commands received"));
        test::require_equal(
            expected.commands_processed,
            actual.commands_processed,
            std::string(label).append(" commands processed"));
        test::require_equal(
            expected.commands_accepted,
            actual.commands_accepted,
            std::string(label).append(" commands accepted"));
        test::require_equal(
            expected.commands_rejected,
            actual.commands_rejected,
            std::string(label).append(" commands rejected"));
        test::require_equal(
            expected.command_results.size(),
            actual.command_results.size(),
            std::string(label).append(" command result count"));
        for (std::size_t index = 0; index < expected.command_results.size(); ++index)
        {
            const auto result_label = std::string(label)
                .append(" command result ")
                .append(std::to_string(index));
            test::require_equal(
                expected.command_results[index].sequence.value,
                actual.command_results[index].sequence.value,
                result_label + " sequence");
            test::require_equal(
                expected.command_results[index].reason,
                actual.command_results[index].reason,
                result_label + " reason");
        }
        test::require_equal(expected.active_zones, actual.active_zones, std::string(label).append(" active zones"));
        test::require_equal(expected.entities_considered, actual.entities_considered, std::string(label).append(" entities considered"));
        test::require_equal(expected.entities_skipped, actual.entities_skipped, std::string(label).append(" entities skipped"));
        test::require_equal(expected.load_recalculations, actual.load_recalculations, std::string(label).append(" load recalculations"));
        test::require_equal(expected.status_sweeps, actual.status_sweeps, std::string(label).append(" status sweeps"));
        test::require_equal(expected.status_changes, actual.status_changes, std::string(label).append(" status changes"));
        test::require_equal(
            expected.status_periodic_applications,
            actual.status_periodic_applications,
            std::string(label).append(" periodic applications"));
    }

    auto require_equivalent_worlds(
        const mmo::world::World& expected,
        const mmo::world::World& actual,
        std::uint32_t zone_count) -> void
    {
        const auto expected_ids = mmo::world::list_entity_ids_in_simulation_order(expected);
        const auto actual_ids = mmo::world::list_entity_ids_in_simulation_order(actual);
        test::require(expected_ids == actual_ids, "canonical entity ids should match");

        for (const auto entity_id : expected_ids)
        {
            const auto* expected_record = expected.find_entity(entity_id);
            const auto* actual_record = actual.find_entity(entity_id);
            test::require(expected_record != nullptr && actual_record != nullptr, "equivalent entity should exist");
            test::require_equal(expected_record->placement.zone_id(), actual_record->placement.zone_id(), "zone id");
            test::require_equal(expected_record->resources.health_current, actual_record->resources.health_current, "health");
            test::require_equal(expected_record->resources.mana_current, actual_record->resources.mana_current, "mana");
            test::require_equal(expected_record->resources.shield_current, actual_record->resources.shield_current, "shield");
            test::require_equal(expected_record->lifecycle.alive, actual_record->lifecycle.alive, "alive state");
            test::require_equal(expected_record->load.carried_weight, actual_record->load.carried_weight, "carried weight");
            test::require_equal(expected_record->load.inventory_dirty, actual_record->load.inventory_dirty, "load dirty state");
            test::require_equal(
                expected_record->combat.status_effects.size(),
                actual_record->combat.status_effects.size(),
                "active status count");
            test::require_equal(
                expected_record->combat.status_effects.pending_size(),
                actual_record->combat.status_effects.pending_size(),
                "pending status count");

            const auto* expected_poison = expected_record->combat.status_effects.find(
                mmo::core::status::Kind::poison);
            const auto* actual_poison = actual_record->combat.status_effects.find(
                mmo::core::status::Kind::poison);
            test::require(
                (expected_poison == nullptr) == (actual_poison == nullptr),
                "poison presence should match");
            if (expected_poison != nullptr && actual_poison != nullptr)
            {
                test::require_equal(expected_poison->stacks, actual_poison->stacks, "poison stacks");
                test::require(
                    expected_poison->next_tick_at == actual_poison->next_tick_at,
                    "poison deadline should match");
                test::require(
                    expected_poison->expires_at == actual_poison->expires_at,
                    "poison expiration should match");
            }
        }

        for (std::uint32_t zone = 1; zone <= zone_count; ++zone)
        {
            const auto zone_id = static_cast<mmo::core::id::ZoneId>(zone);
            const auto* expected_zone = expected.find_zone(zone_id);
            const auto* actual_zone = actual.find_zone(zone_id);
            test::require(expected_zone != nullptr && actual_zone != nullptr, "equivalent zone should exist");
            test::require_equal(expected_zone->last_tick, actual_zone->last_tick, "zone last tick");
            test::require_equal(
                expected_zone->wake_requested,
                actual_zone->wake_requested,
                "zone explicit wake state");
            test::require_equal(expected_zone->player_count, actual_zone->player_count, "zone player count");
            test::require_equal(expected_zone->entity_count, actual_zone->entity_count, "zone entity count");
            test::require_equal(
                expected_zone->is_active(),
                actual_zone->is_active(),
                "zone active state");
        }

        test::require(
            expected.active_zone_ids() == actual.active_zone_ids(),
            "active zone indexes should match");

        test::require_equal(expected.pending_event_count(), actual.pending_event_count(), "pending event count");
        test::require_equal(expected.pending_events().size(), actual.pending_events().size(), "event outbox size");
        test::require_equal(
            expected.pending_rejected_events().size(),
            actual.pending_rejected_events().size(),
            "rejected event size");
        for (std::size_t index = 0; index < expected.pending_events().size(); ++index)
        {
            test::require_equal(
                expected.pending_events()[index].counter,
                actual.pending_events()[index].counter,
                "event outbox counter");
        }
    }

    auto test_empty_tick(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 0, epoch() },
            mmo::world::command::Batch{ 0 });

        test::require_equal(static_cast<std::uint64_t>(0), stats.events_processed, "empty events");
        test::require_equal(static_cast<std::uint64_t>(0), stats.entities_considered, "empty entities");
        test::require_equal(static_cast<std::uint64_t>(0), stats.status_sweeps, "empty status sweeps");
        test::require_equal(static_cast<std::uint64_t>(0), stats.status_changes, "empty status changes");
        details = "entities=0 events=0 statuses=0";
    }

    auto test_due_event_exactly_once(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        mmo::core::event::Event event{};
        event.type = mmo::core::event::Type::region_notice;
        event.due_at = epoch();
        event.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        event.counter = 77;
        test::require(world.try_schedule_event(event), "due event should schedule");

        const auto first = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 0, epoch() },
            mmo::world::command::Batch{ 0 });
        const auto second = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 1, epoch() },
            mmo::world::command::Batch{ 1 });

        test::require_equal(static_cast<std::uint64_t>(1), first.events_processed, "first event processing");
        test::require_equal(static_cast<std::uint64_t>(1), first.events_queued, "first event queued");
        test::require_equal(static_cast<std::uint64_t>(0), second.events_processed, "event not repeated");
        test::require(world.event_queue_empty(), "due scheduler should be empty");
        test::require_equal(static_cast<std::size_t>(1), world.pending_events().size(), "outbox exactly once");
        details = "processed=1 repeated=0";
    }

    auto test_future_event_remains_pending(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        mmo::core::event::Event event{};
        event.type = mmo::core::event::Type::region_notice;
        event.due_at = epoch() + mmo::core::time::Milliseconds{ 100 };
        event.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        test::require(world.try_schedule_event(event), "future event should schedule");

        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 0, epoch() + mmo::core::time::Milliseconds{ 99 } },
            mmo::world::command::Batch{ 0 });

        test::require_equal(static_cast<std::uint64_t>(0), stats.events_processed, "future event processing");
        test::require_equal(static_cast<std::size_t>(1), world.pending_event_count(), "future event pending");
        test::require(world.next_event_due() == event.due_at, "future deadline should be observable");
        details = "processed=0 pending=1";
    }

    auto test_validated_scheduler_boundary(std::string& details) -> void
    {
        mmo::world::World world{};
        mmo::core::event::Event invalid{};
        invalid.type = mmo::core::event::Type::migration_completed;
        invalid.due_at = epoch();

        test::require(!world.try_schedule_event(invalid), "invalid event should be rejected before scheduling");
        test::require(world.event_queue_empty(), "invalid event must not enter normal queue");
        test::require(!world.next_event_due().has_value(), "invalid event must not create a deadline");
        details = "invalid_migration queued=0";
    }

    auto test_event_drain_preserves_order(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};

        mmo::core::event::Event first{};
        first.type = mmo::core::event::Type::region_notice;
        first.due_at = epoch();
        first.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        first.counter = 10;
        auto second = first;
        second.counter = 20;

        test::require(world.try_schedule_event(first), "first output event should schedule");
        test::require(world.try_schedule_event(second), "second output event should schedule");
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 0, epoch() },
            mmo::world::command::Batch{ 0 });

        test::require_equal(static_cast<std::uint64_t>(2), stats.events_queued, "queued output count");
        const auto drained = world.drain_events();
        test::require_equal(static_cast<std::size_t>(2), drained.size(), "first event drain size");
        test::require_equal(static_cast<std::uint32_t>(10), drained[0].counter, "first drained event");
        test::require_equal(static_cast<std::uint32_t>(20), drained[1].counter, "second drained event");
        test::require(world.pending_events().empty(), "event outbox should be empty after drain");
        test::require(world.drain_events().empty(), "second event drain should be empty");
        details = "produced=2 drained=2 order=10,20 second_drain=0";
    }

    auto test_health_mutation_uses_simulation_time(std::string& details) -> void
    {
        mmo::world::World first{};
        mmo::world::World second{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        spawn_entity(first, entity_id);
        spawn_entity(second, entity_id);
        const auto logical_time = epoch() + mmo::core::time::Milliseconds{ 1234 };

        test::require(
            first.adjust_health(entity_id, std::numeric_limits<std::int32_t>::min(), logical_time),
            "first deterministic health mutation");
        test::require(
            second.adjust_health(entity_id, std::numeric_limits<std::int32_t>::min(), logical_time),
            "second deterministic health mutation");

        const auto* first_record = first.find_entity(entity_id);
        const auto* second_record = second.find_entity(entity_id);
        test::require(first_record != nullptr && second_record != nullptr, "deterministic entities should exist");
        test::require_equal(
            first_record->resources.health_current,
            second_record->resources.health_current,
            "deterministic health");
        test::require_equal(first_record->lifecycle.alive, second_record->lifecycle.alive, "deterministic alive state");
        test::require(first_record->lifecycle.death_at == logical_time, "first logical death time");
        test::require(second_record->lifecycle.death_at == logical_time, "second logical death time");
        details = "worlds=2 hp=0 alive=false death_at=logical_time";
    }

    auto test_read_only_identity_preserves_spatial_state(std::string& details) -> void
    {
        mmo::world::World world{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        constexpr auto zone_id = static_cast<mmo::core::id::ZoneId>(9);
        spawn_entity(world, entity_id, zone_id);

        const auto* record = world.find_entity(entity_id);
        const auto* zone = world.find_zone(zone_id);
        test::require(record != nullptr && zone != nullptr, "read-only spatial views should exist");
        test::require_equal(mmo::core::entity::Type::player, record->identity.type, "read-only identity type");
        test::require_equal(static_cast<std::uint64_t>(1), zone->player_count, "zone player count");
        test::require(world.should_tick_full(zone_id), "player zone should remain active");
        test::require(world.has_consistent_spatial_state(), "read-only access must preserve spatial state");
        details = "lookup=const identity=player players=1 active=true";
    }

    auto test_dirty_load_recalculates_once(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        spawn_entity(world, static_cast<mmo::core::id::EntityId>(1));

        const auto first = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 7, epoch() },
            mmo::world::command::Batch{ 7 });
        const auto second = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 8, epoch() + mmo::core::time::Milliseconds{ 50 } },
            mmo::world::command::Batch{ 8 });

        test::require_equal(static_cast<std::uint64_t>(1), first.load_recalculations, "dirty load sync");
        test::require_equal(static_cast<std::uint64_t>(0), second.load_recalculations, "clean load reuse");
        const auto* zone = world.find_zone(static_cast<mmo::core::id::ZoneId>(1));
        test::require(zone != nullptr, "entity zone should be tracked");
        test::require_equal(static_cast<mmo::core::time::TickCount>(8), zone->last_tick, "zone tick index");
        details = "load_recalculations=1/0";
    }

    auto test_periodic_status_cadence(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        spawn_entity(world, entity_id);
        const auto initial_health = world.find_entity(entity_id)->resources.health_current;
        apply_poison(world, entity_id);

        const auto before = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 19, epoch() + mmo::core::time::Milliseconds{ 999 } },
            mmo::world::command::Batch{ 19 });
        const auto first = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 20, epoch() + mmo::core::time::Milliseconds{ 1000 } },
            mmo::world::command::Batch{ 20 });
        const auto second = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 40, epoch() + mmo::core::time::Milliseconds{ 2000 } },
            mmo::world::command::Batch{ 40 });

        test::require_equal(static_cast<std::uint64_t>(0), before.status_periodic_applications, "before deadline");
        test::require_equal(static_cast<std::uint64_t>(1), first.status_periodic_applications, "first deadline");
        test::require_equal(static_cast<std::uint64_t>(1), second.status_periodic_applications, "second deadline");
        test::require_equal(
            initial_health - 16,
            world.find_entity(entity_id)->resources.health_current,
            "periodic health delta");
        details = "before=0 deadline1=1 deadline2=1 damage=16";
    }

    auto test_periodic_resource_invariants(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        spawn_entity(world, entity_id);

        auto lethal_definition = mmo::core::status::make_poison_definition();
        lethal_definition.delivery = mmo::core::status::Delivery::instant;
        lethal_definition.duration = mmo::core::time::Milliseconds{ 1000 };
        lethal_definition.tick_interval = mmo::core::time::Milliseconds{ 1000 };
        lethal_definition.modifiers.health_delta_per_tick = std::numeric_limits<std::int32_t>::min();
        lethal_definition.modifiers.mana_delta_per_tick = std::numeric_limits<std::int32_t>::min();
        const auto lethal = mmo::core::status::make_instance(lethal_definition, epoch());
        test::require(world.apply_status(entity_id, lethal), "lethal periodic should apply");

        const auto shield = mmo::core::status::make_instance(
            mmo::core::status::make_shield_definition(),
            epoch());
        test::require(world.apply_status(entity_id, shield), "shield should apply");
        test::require(world.find_entity(entity_id)->resources.shield_current > 0, "shield points expected");

        const auto deadline = epoch() + mmo::core::time::Milliseconds{ 1000 };
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 20, deadline },
            mmo::world::command::Batch{ 20 });
        const auto* record = world.find_entity(entity_id);

        test::require_equal(static_cast<std::uint64_t>(1), stats.status_periodic_applications, "lethal application count");
        test::require_equal(static_cast<std::int32_t>(0), record->resources.health_current, "health lower clamp");
        test::require_equal(static_cast<std::int32_t>(0), record->resources.mana_current, "mana lower clamp");
        test::require_equal(static_cast<std::int32_t>(0), record->resources.shield_current, "shield consumption");
        test::require(!record->lifecycle.alive, "lethal periodic should kill");
        test::require(record->lifecycle.death_at == deadline, "death time should use simulation clock");
        details = "applications=1 hp=0 mana=0 shield=0 deterministic_death_time=true";
    }

    auto test_status_expiration_boundary(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        spawn_entity(world, entity_id);
        const auto base_move_speed = world.find_entity(entity_id)->stats.current_derived.move_speed;
        const auto haste = mmo::core::status::make_instance(
            mmo::core::status::make_haste_definition(),
            epoch());
        test::require(world.apply_status(entity_id, haste), "haste should apply");
        test::require(
            world.find_entity(entity_id)->stats.current_derived.move_speed > base_move_speed,
            "haste should affect derived stats");

        const auto before = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 119, epoch() + mmo::core::time::Milliseconds{ 5999 } },
            mmo::world::command::Batch{ 119 });
        const auto at_deadline = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 120, epoch() + mmo::core::time::Milliseconds{ 6000 } },
            mmo::world::command::Batch{ 120 });

        test::require_equal(static_cast<std::uint64_t>(0), before.status_changes, "status before expiration");
        test::require_equal(static_cast<std::uint64_t>(1), at_deadline.status_changes, "status expiration change");
        test::require(
            world.find_entity(entity_id)->combat.status_effects.find(mmo::core::status::Kind::haste) == nullptr,
            "haste should expire at deadline");
        test::require_equal(
            base_move_speed,
            world.find_entity(entity_id)->stats.current_derived.move_speed,
            "derived stats after expiration");
        details = "before_deadline=active at_deadline=expired";
    }

    auto test_canonical_entity_traversal(std::string& details) -> void
    {
        mmo::world::World first{};
        mmo::world::World second{};
        const mmo::core::item::Catalog items{};
        for (const auto id : { 30u, 10u, 20u })
        {
            spawn_entity(first, static_cast<mmo::core::id::EntityId>(id));
        }
        for (const auto id : { 20u, 30u, 10u })
        {
            spawn_entity(second, static_cast<mmo::core::id::EntityId>(id));
        }

        const std::vector<mmo::core::id::EntityId> expected{ 10, 20, 30 };
        test::require(
            mmo::world::list_entity_ids_in_simulation_order(first) == expected,
            "first traversal should be canonical");
        test::require(
            mmo::world::list_entity_ids_in_simulation_order(second) == expected,
            "second traversal should ignore insertion order");

        const auto first_stats = mmo::world::step(
            first,
            items,
            mmo::world::TickContext{ 42, epoch() },
            mmo::world::command::Batch{ 42 });
        const auto second_stats = mmo::world::step(
            second,
            items,
            mmo::world::TickContext{ 42, epoch() },
            mmo::world::command::Batch{ 42 });
        require_equal_stats(first_stats, second_stats, "canonical traversal");
        details = "insertions=30,10,20/20,30,10 traversal=10,20,30";
    }

    auto build_repeated_world(const std::vector<mmo::core::id::EntityId>& ids)
        -> mmo::world::World
    {
        mmo::world::World world{};
        for (const auto entity_id : ids)
        {
            const auto zone_id = static_cast<mmo::core::id::ZoneId>((entity_id % 4) + 1);
            spawn_entity(world, entity_id, zone_id);
            if ((entity_id % 3) == 0)
            {
                apply_poison(world, entity_id);
            }
        }

        mmo::core::event::Event due{};
        due.type = mmo::core::event::Type::region_notice;
        due.due_at = epoch() + mmo::core::time::Milliseconds{ 1000 };
        due.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        due.counter = 1;
        test::require(world.try_schedule_event(due), "repeated due event should schedule");

        auto future = due;
        future.due_at = epoch() + mmo::core::time::Milliseconds{ 10000 };
        future.counter = 2;
        test::require(world.try_schedule_event(future), "repeated future event should schedule");
        return world;
    }

    auto test_repeated_simulation_is_identical(std::string& details) -> void
    {
        constexpr std::uint32_t entity_count = 128;
        constexpr std::uint32_t tick_count = 100;
        std::vector<mmo::core::id::EntityId> ascending;
        ascending.reserve(entity_count);
        for (std::uint32_t value = 1; value <= entity_count; ++value)
        {
            ascending.push_back(static_cast<mmo::core::id::EntityId>(value));
        }
        auto descending = ascending;
        std::reverse(descending.begin(), descending.end());

        auto first = build_repeated_world(ascending);
        auto second = build_repeated_world(descending);
        const mmo::core::item::Catalog items{};
        mmo::world::TickStats first_totals{};
        mmo::world::TickStats second_totals{};

        for (std::uint32_t tick = 0; tick < tick_count; ++tick)
        {
            const auto context = mmo::world::TickContext{
                tick,
                epoch() + mmo::core::time::Milliseconds{ 50 * tick }
            };
            const auto first_tick = mmo::world::step(
                first,
                items,
                context,
                mmo::world::command::Batch{ context.tick_index });
            const auto second_tick = mmo::world::step(
                second,
                items,
                context,
                mmo::world::command::Batch{ context.tick_index });
            require_equal_stats(first_tick, second_tick, "repeated tick");
            accumulate_stats(first_totals, first_tick);
            accumulate_stats(second_totals, second_tick);
        }

        require_equal_stats(first_totals, second_totals, "repeated totals");
        require_equivalent_worlds(first, second, 4);
        test::require_equal(
            static_cast<std::uint64_t>(entity_count) * tick_count,
            first_totals.entities_considered,
            "repeated entity operations");
        test::require_equal(static_cast<std::size_t>(1), first.pending_event_count(), "future event remains");
        details = "worlds=2 entities_per_world=128 ticks=100 entity_considerations=25600";
    }

    auto test_authoritative_spatial_mutations(std::string& details) -> void
    {
        mmo::world::World world{};
        constexpr auto player_id = static_cast<mmo::core::id::EntityId>(10);
        constexpr auto monster_id = static_cast<mmo::core::id::EntityId>(20);
        constexpr auto first_zone = static_cast<mmo::core::id::ZoneId>(10);
        constexpr auto second_zone = static_cast<mmo::core::id::ZoneId>(20);

        spawn_entity(
            world,
            monster_id,
            second_zone,
            mmo::core::entity::Type::monster);
        spawn_entity(world, player_id, first_zone);

        const auto* first = world.find_zone(first_zone);
        const auto* second = world.find_zone(second_zone);
        test::require(first != nullptr && second != nullptr, "spawned zones should exist");
        test::require_equal(static_cast<std::uint64_t>(1), first->entity_count, "first zone entities");
        test::require_equal(static_cast<std::uint64_t>(1), first->player_count, "first zone players");
        test::require(first->is_active(), "player zone should be active");
        test::require(!second->is_active(), "monster-only zone should sleep");
        test::require(world.has_consistent_spatial_state(), "spawned spatial state");

        test::require(
            world.move_entity_to_zone(player_id, second_zone, epoch()),
            "player migration should succeed");
        first = world.find_zone(first_zone);
        second = world.find_zone(second_zone);
        test::require(first != nullptr && second != nullptr, "migrated zones should exist");
        test::require_equal(static_cast<std::uint64_t>(0), first->entity_count, "source entities after move");
        test::require(!first->is_active(), "empty source zone should sleep");
        test::require_equal(static_cast<std::uint64_t>(2), second->entity_count, "destination entities after move");
        test::require_equal(static_cast<std::uint64_t>(1), second->player_count, "destination players after move");
        test::require(second->is_active(), "destination with player should be active");
        test::require(world.has_consistent_spatial_state(), "migrated spatial state");

        test::require(world.erase_entity(player_id, epoch()), "player erase should succeed");
        second = world.find_zone(second_zone);
        test::require(second != nullptr, "destination zone should remain known");
        test::require_equal(static_cast<std::uint64_t>(1), second->entity_count, "destination entities after erase");
        test::require_equal(static_cast<std::uint64_t>(0), second->player_count, "destination players after erase");
        test::require(!second->is_active(), "monster-only destination should sleep again");
        test::require(world.active_zone_ids().empty(), "no active zones should remain");
        test::require(world.has_consistent_spatial_state(), "erased spatial state");
        details = "spawn/move/erase populations consistent; active_zones=0";
    }

    auto test_sparse_active_set(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto active_zone = static_cast<mmo::core::id::ZoneId>(1);
        constexpr auto sleeping_zone = static_cast<mmo::core::id::ZoneId>(2);
        spawn_entity(world, static_cast<mmo::core::id::EntityId>(100), active_zone);
        spawn_entity(
            world,
            static_cast<mmo::core::id::EntityId>(1),
            sleeping_zone,
            mmo::core::entity::Type::monster);

        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 7, epoch() },
            mmo::world::command::Batch{ 7 });

        test::require_equal(static_cast<std::uint64_t>(1), stats.active_zones, "active zone count");
        test::require_equal(static_cast<std::uint64_t>(1), stats.entities_considered, "active entities");
        test::require_equal(static_cast<std::uint64_t>(1), stats.entities_skipped, "sleeping entities");
        test::require_equal(static_cast<std::uint64_t>(1), stats.status_sweeps, "active status sweeps");
        test::require_equal(
            static_cast<mmo::core::time::TickCount>(7),
            world.find_zone(active_zone)->last_tick,
            "active zone tick");
        test::require_equal(
            static_cast<mmo::core::time::TickCount>(0),
            world.find_zone(sleeping_zone)->last_tick,
            "sleeping zone tick");
        details = "known=2 active=1 considered=1 skipped=1";
    }

    auto test_canonical_active_order(std::string& details) -> void
    {
        mmo::world::World first{};
        mmo::world::World second{};
        constexpr auto first_zone = static_cast<mmo::core::id::ZoneId>(10);
        constexpr auto second_zone = static_cast<mmo::core::id::ZoneId>(20);

        spawn_entity(first, 30, first_zone, mmo::core::entity::Type::monster);
        spawn_entity(first, 20, first_zone);
        spawn_entity(first, 1, second_zone);

        spawn_entity(second, 1, second_zone);
        spawn_entity(second, 20, first_zone);
        spawn_entity(second, 30, first_zone, mmo::core::entity::Type::monster);

        const std::vector<mmo::core::id::EntityId> expected{ 20, 30, 1 };
        test::require(
            mmo::world::list_active_entity_ids_in_simulation_order(first) == expected,
            "first active order should be zone/entity canonical");
        test::require(
            mmo::world::list_active_entity_ids_in_simulation_order(second) == expected,
            "second active order should ignore insertion order");
        details = "canonical_order=(zone 10: 20,30),(zone 20: 1)";
    }

    auto test_wake_and_sleep_events_change_same_tick_set(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto zone_id = static_cast<mmo::core::id::ZoneId>(5);
        spawn_entity(
            world,
            static_cast<mmo::core::id::EntityId>(1),
            zone_id,
            mmo::core::entity::Type::monster);

        mmo::core::event::Event wake{};
        wake.type = mmo::core::event::Type::zone_wake;
        wake.due_at = epoch();
        wake.zone_id = zone_id;
        test::require(world.try_schedule_event(wake), "wake event should schedule");
        const auto awake = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 4, epoch() },
            mmo::world::command::Batch{ 4 });
        test::require_equal(static_cast<std::uint64_t>(1), awake.events_applied, "wake applied");
        test::require_equal(static_cast<std::uint64_t>(1), awake.entities_considered, "wake same tick entity");
        test::require(world.should_tick_full(zone_id), "woken zone should be active");

        auto sleep = wake;
        sleep.type = mmo::core::event::Type::zone_sleep;
        sleep.due_at = epoch() + mmo::core::time::Milliseconds{ 1 };
        test::require(world.try_schedule_event(sleep), "sleep event should schedule");
        const auto asleep = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 5, sleep.due_at },
            mmo::world::command::Batch{ 5 });
        test::require_equal(static_cast<std::uint64_t>(1), asleep.events_applied, "sleep applied");
        test::require_equal(static_cast<std::uint64_t>(0), asleep.entities_considered, "sleep same tick entity");
        test::require_equal(static_cast<std::uint64_t>(1), asleep.entities_skipped, "sleep skipped entity");
        test::require(!world.should_tick_full(zone_id), "slept zone should be inactive");
        details = "wake considered=1; sleep considered=0";
    }

    auto test_player_rejects_sleep(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto zone_id = static_cast<mmo::core::id::ZoneId>(6);
        spawn_entity(world, static_cast<mmo::core::id::EntityId>(1), zone_id);

        mmo::core::event::Event sleep{};
        sleep.type = mmo::core::event::Type::zone_sleep;
        sleep.due_at = epoch();
        sleep.zone_id = zone_id;
        sleep.counter = 1;
        auto second_sleep = sleep;
        second_sleep.counter = 2;
        test::require(world.try_schedule_event(sleep), "player-zone sleep should schedule");
        test::require(world.try_schedule_event(second_sleep), "second player-zone sleep should schedule");
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 1, epoch() },
            mmo::world::command::Batch{ 1 });

        test::require_equal(static_cast<std::uint64_t>(2), stats.events_rejected, "sleep rejection");
        const auto rejected = world.drain_rejected_events();
        test::require_equal(static_cast<std::size_t>(2), rejected.size(), "rejected drain size");
        test::require_equal(static_cast<std::uint32_t>(1), rejected[0].counter, "first rejected event");
        test::require_equal(static_cast<std::uint32_t>(2), rejected[1].counter, "second rejected event");
        test::require(world.pending_rejected_events().empty(), "rejected outbox should be empty after drain");
        test::require(world.drain_rejected_events().empty(), "second rejected drain should be empty");
        test::require_equal(static_cast<std::uint64_t>(1), stats.entities_considered, "player zone remains active");
        test::require(world.should_tick_full(zone_id), "player must keep zone active");
        details = "sleep_rejected=2 drained=2 second_drain=0 active_entities=1";
    }

    auto test_sleep_wake_status_catch_up_is_bounded(std::string& details) -> void
    {
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        constexpr auto zone_id = static_cast<mmo::core::id::ZoneId>(7);
        spawn_entity(world, entity_id, zone_id, mmo::core::entity::Type::monster);
        apply_poison(world, entity_id);
        const auto initial_health = world.find_entity(entity_id)->resources.health_current;

        const auto sleeping = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{
                159,
                epoch() + mmo::core::time::Milliseconds{ 7999 }
            },
            mmo::world::command::Batch{ 159 });
        test::require_equal(static_cast<std::uint64_t>(0), sleeping.status_periodic_applications, "sleeping periodic work");
        test::require_equal(static_cast<std::uint64_t>(1), sleeping.entities_skipped, "sleeping status entity");

        mmo::core::event::Event wake{};
        wake.type = mmo::core::event::Type::zone_wake;
        wake.due_at = epoch() + mmo::core::time::Milliseconds{ 8000 };
        wake.zone_id = zone_id;
        test::require(world.try_schedule_event(wake), "status wake should schedule");
        const auto awakened = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 160, wake.due_at },
            mmo::world::command::Batch{ 160 });

        test::require_equal(
            mmo::world::max_periodic_catch_up_applications_per_status,
            awakened.status_periodic_applications,
            "bounded periodic catch-up");
        test::require_equal(static_cast<std::uint64_t>(1), awakened.status_changes, "absolute expiration sweep");
        test::require_equal(
            initial_health - 32,
            world.find_entity(entity_id)->resources.health_current,
            "bounded catch-up damage");
        test::require(
            world.find_entity(entity_id)->combat.status_effects.find(mmo::core::status::Kind::poison) == nullptr,
            "expired poison should be removed on wake");
        details = "due=8 applied_cap=4 expired=1";
    }

    auto test_sparse_simulation_scale(std::string& details) -> void
    {
        constexpr std::uint32_t active_entities = 64;
        constexpr std::uint32_t sleeping_entities = 960;
        constexpr std::uint32_t tick_count = 100;
        mmo::world::World world{};
        const mmo::core::item::Catalog items{};

        spawn_entity(world, static_cast<mmo::core::id::EntityId>(1));
        for (std::uint32_t value = 2; value <= active_entities; ++value)
        {
            spawn_entity(
                world,
                static_cast<mmo::core::id::EntityId>(value),
                static_cast<mmo::core::id::ZoneId>(1),
                mmo::core::entity::Type::monster);
        }

        for (std::uint32_t offset = 0; offset < sleeping_entities; ++offset)
        {
            const auto entity_id = static_cast<mmo::core::id::EntityId>(
                active_entities + offset + 1);
            const auto zone_id = static_cast<mmo::core::id::ZoneId>(
                2 + (offset % 15));
            spawn_entity(
                world,
                entity_id,
                zone_id,
                mmo::core::entity::Type::monster);
        }

        mmo::world::TickStats totals{};
        for (std::uint32_t tick = 0; tick < tick_count; ++tick)
        {
            const auto stats = mmo::world::step(
                world,
                items,
                mmo::world::TickContext{
                    tick,
                    epoch() + mmo::core::time::Milliseconds{ 50 * tick }
                },
                mmo::world::command::Batch{ tick });
            accumulate_stats(totals, stats);
        }

        test::require_equal(
            static_cast<std::uint64_t>(active_entities) * tick_count,
            totals.entities_considered,
            "scaled active work");
        test::require_equal(
            static_cast<std::uint64_t>(sleeping_entities) * tick_count,
            totals.entities_skipped,
            "scaled sleeping work avoided");
        test::require_equal(
            static_cast<std::uint64_t>(tick_count),
            totals.active_zones,
            "scaled active zone work");
        test::require(world.has_consistent_spatial_state(), "scaled spatial state");
        details = "known=1024 active=64 sleeping=960 ticks=100 considered=6400 skipped=96000";
    }
}

int main()
{
    std::vector<test::TestResult> results;
    results.push_back(test::run_test("simulation.empty_tick", test_empty_tick));
    results.push_back(test::run_test("simulation.due_event_exactly_once", test_due_event_exactly_once));
    results.push_back(test::run_test("simulation.future_event_pending", test_future_event_remains_pending));
    results.push_back(test::run_test("simulation.validated_scheduler", test_validated_scheduler_boundary));
    results.push_back(test::run_test("simulation.event_drain_order", test_event_drain_preserves_order));
    results.push_back(test::run_test("simulation.logical_health_time", test_health_mutation_uses_simulation_time));
    results.push_back(test::run_test("simulation.read_only_identity", test_read_only_identity_preserves_spatial_state));
    results.push_back(test::run_test("simulation.dirty_load_once", test_dirty_load_recalculates_once));
    results.push_back(test::run_test("simulation.periodic_cadence", test_periodic_status_cadence));
    results.push_back(test::run_test("simulation.periodic_resource_invariants", test_periodic_resource_invariants));
    results.push_back(test::run_test("simulation.status_expiration", test_status_expiration_boundary));
    results.push_back(test::run_test("simulation.canonical_traversal", test_canonical_entity_traversal));
    results.push_back(test::run_test("simulation.repeated_identical_worlds", test_repeated_simulation_is_identical));
    results.push_back(test::run_test("simulation.authoritative_spatial_mutations", test_authoritative_spatial_mutations));
    results.push_back(test::run_test("simulation.sparse_active_set", test_sparse_active_set));
    results.push_back(test::run_test("simulation.canonical_active_order", test_canonical_active_order));
    results.push_back(test::run_test("simulation.wake_sleep_same_tick", test_wake_and_sleep_events_change_same_tick_set));
    results.push_back(test::run_test("simulation.player_rejects_sleep", test_player_rejects_sleep));
    results.push_back(test::run_test("simulation.sleep_wake_status_catch_up", test_sleep_wake_status_catch_up_is_bounded));
    results.push_back(test::run_test("simulation.sparse_scale", test_sparse_simulation_scale));
    return test::report_results("Simulation integration", results, std::cout);
}
