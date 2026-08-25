#include "mmo/tests/support/test.hpp"
#include "mmo/world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    namespace test = mmo::tests::support;

    constexpr auto epoch() -> mmo::core::time::TimePoint
    {
        return mmo::core::time::TimePoint{};
    }

    auto make_blueprint() -> mmo::core::entity::Blueprint
    {
        mmo::core::entity::Blueprint blueprint{};
        blueprint.template_id = static_cast<mmo::core::id::EntityTemplateId>(1);
        blueprint.species_id = static_cast<mmo::core::id::SpeciesId>(1);
        blueprint.type = mmo::core::entity::Type::player;
        blueprint.display_name = "Simulation Entity";
        blueprint.base_primary = mmo::core::stat::Primary{ 12, 12, 8, 6, 5, 7, 4 };
        blueprint.level = 1;
        return blueprint;
    }

    auto spawn_entity(
        mmo::core::runtime::World& world,
        mmo::core::id::EntityId entity_id,
        mmo::core::id::ZoneId zone_id = static_cast<mmo::core::id::ZoneId>(1)) -> void
    {
        test::require(
            world.entities.spawn(entity_id, make_blueprint(), zone_id, epoch()),
            "simulation entity should spawn");
    }

    auto apply_poison(
        mmo::core::runtime::World& world,
        mmo::core::id::EntityId entity_id) -> void
    {
        const auto poison = mmo::core::status::make_instance(
            mmo::core::status::make_poison_definition(),
            epoch(),
            1,
            entity_id);
        test::require(
            world.entities.apply_status(entity_id, poison),
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
        totals.entities_considered += tick.entities_considered;
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
        test::require_equal(expected.entities_considered, actual.entities_considered, std::string(label).append(" entities considered"));
        test::require_equal(expected.load_recalculations, actual.load_recalculations, std::string(label).append(" load recalculations"));
        test::require_equal(expected.status_sweeps, actual.status_sweeps, std::string(label).append(" status sweeps"));
        test::require_equal(expected.status_changes, actual.status_changes, std::string(label).append(" status changes"));
        test::require_equal(
            expected.status_periodic_applications,
            actual.status_periodic_applications,
            std::string(label).append(" periodic applications"));
    }

    auto require_equivalent_worlds(
        const mmo::core::runtime::World& expected,
        const mmo::core::runtime::World& actual,
        std::uint32_t zone_count) -> void
    {
        const auto expected_ids = mmo::world::list_entity_ids_in_simulation_order(expected);
        const auto actual_ids = mmo::world::list_entity_ids_in_simulation_order(actual);
        test::require(expected_ids == actual_ids, "canonical entity ids should match");

        for (const auto entity_id : expected_ids)
        {
            const auto* expected_record = expected.entities.find(entity_id);
            const auto* actual_record = actual.entities.find(entity_id);
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
            const auto* expected_zone = expected.zones.get(zone_id);
            const auto* actual_zone = actual.zones.get(zone_id);
            test::require(expected_zone != nullptr && actual_zone != nullptr, "equivalent zone should exist");
            test::require_equal(expected_zone->last_tick, actual_zone->last_tick, "zone last tick");
            test::require_equal(expected_zone->active, actual_zone->active, "zone active state");
        }

        test::require_equal(expected.scheduler.size(), actual.scheduler.size(), "pending event count");
        test::require_equal(expected.event_outbox.size(), actual.event_outbox.size(), "event outbox size");
        test::require_equal(expected.rejected_events.size(), actual.rejected_events.size(), "rejected event size");
        for (std::size_t index = 0; index < expected.event_outbox.size(); ++index)
        {
            test::require_equal(
                expected.event_outbox[index].counter,
                actual.event_outbox[index].counter,
                "event outbox counter");
        }
    }

    auto test_empty_tick(std::string& details) -> void
    {
        mmo::core::runtime::World world{};
        const mmo::core::item::Catalog items{};
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 0, epoch() });

        test::require_equal(static_cast<std::uint64_t>(0), stats.events_processed, "empty events");
        test::require_equal(static_cast<std::uint64_t>(0), stats.entities_considered, "empty entities");
        test::require_equal(static_cast<std::uint64_t>(0), stats.status_sweeps, "empty status sweeps");
        test::require_equal(static_cast<std::uint64_t>(0), stats.status_changes, "empty status changes");
        details = "entities=0 events=0 statuses=0";
    }

    auto test_due_event_exactly_once(std::string& details) -> void
    {
        mmo::core::runtime::World world{};
        const mmo::core::item::Catalog items{};
        mmo::core::event::Event event{};
        event.type = mmo::core::event::Type::region_notice;
        event.due_at = epoch();
        event.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        event.counter = 77;
        test::require(world.scheduler.try_schedule(event), "due event should schedule");

        const auto first = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 0, epoch() });
        const auto second = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 1, epoch() });

        test::require_equal(static_cast<std::uint64_t>(1), first.events_processed, "first event processing");
        test::require_equal(static_cast<std::uint64_t>(1), first.events_queued, "first event queued");
        test::require_equal(static_cast<std::uint64_t>(0), second.events_processed, "event not repeated");
        test::require(world.scheduler.empty(), "due scheduler should be empty");
        test::require_equal(static_cast<std::size_t>(1), world.event_outbox.size(), "outbox exactly once");
        details = "processed=1 repeated=0";
    }

    auto test_future_event_remains_pending(std::string& details) -> void
    {
        mmo::core::runtime::World world{};
        const mmo::core::item::Catalog items{};
        mmo::core::event::Event event{};
        event.type = mmo::core::event::Type::region_notice;
        event.due_at = epoch() + mmo::core::time::Milliseconds{ 100 };
        event.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        test::require(world.scheduler.try_schedule(event), "future event should schedule");

        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 0, epoch() + mmo::core::time::Milliseconds{ 99 } });

        test::require_equal(static_cast<std::uint64_t>(0), stats.events_processed, "future event processing");
        test::require_equal(static_cast<std::size_t>(1), world.scheduler.size(), "future event pending");
        details = "processed=0 pending=1";
    }

    auto test_dirty_load_recalculates_once(std::string& details) -> void
    {
        mmo::core::runtime::World world{};
        const mmo::core::item::Catalog items{};
        spawn_entity(world, static_cast<mmo::core::id::EntityId>(1));

        const auto first = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 7, epoch() });
        const auto second = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 8, epoch() + mmo::core::time::Milliseconds{ 50 } });

        test::require_equal(static_cast<std::uint64_t>(1), first.load_recalculations, "dirty load sync");
        test::require_equal(static_cast<std::uint64_t>(0), second.load_recalculations, "clean load reuse");
        const auto* zone = world.zones.get(static_cast<mmo::core::id::ZoneId>(1));
        test::require(zone != nullptr, "entity zone should be tracked");
        test::require_equal(static_cast<mmo::core::time::TickCount>(8), zone->last_tick, "zone tick index");
        details = "load_recalculations=1/0";
    }

    auto test_periodic_status_cadence(std::string& details) -> void
    {
        mmo::core::runtime::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        spawn_entity(world, entity_id);
        const auto initial_health = world.entities.find(entity_id)->resources.health_current;
        apply_poison(world, entity_id);

        const auto before = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 19, epoch() + mmo::core::time::Milliseconds{ 999 } });
        const auto first = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 20, epoch() + mmo::core::time::Milliseconds{ 1000 } });
        const auto second = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 40, epoch() + mmo::core::time::Milliseconds{ 2000 } });

        test::require_equal(static_cast<std::uint64_t>(0), before.status_periodic_applications, "before deadline");
        test::require_equal(static_cast<std::uint64_t>(1), first.status_periodic_applications, "first deadline");
        test::require_equal(static_cast<std::uint64_t>(1), second.status_periodic_applications, "second deadline");
        test::require_equal(
            initial_health - 16,
            world.entities.find(entity_id)->resources.health_current,
            "periodic health delta");
        details = "before=0 deadline1=1 deadline2=1 damage=16";
    }

    auto test_periodic_resource_invariants(std::string& details) -> void
    {
        mmo::core::runtime::World world{};
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
        test::require(world.entities.apply_status(entity_id, lethal), "lethal periodic should apply");

        const auto shield = mmo::core::status::make_instance(
            mmo::core::status::make_shield_definition(),
            epoch());
        test::require(world.entities.apply_status(entity_id, shield), "shield should apply");
        test::require(world.entities.find(entity_id)->resources.shield_current > 0, "shield points expected");

        const auto deadline = epoch() + mmo::core::time::Milliseconds{ 1000 };
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 20, deadline });
        const auto* record = world.entities.find(entity_id);

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
        mmo::core::runtime::World world{};
        const mmo::core::item::Catalog items{};
        constexpr auto entity_id = static_cast<mmo::core::id::EntityId>(1);
        spawn_entity(world, entity_id);
        const auto base_move_speed = world.entities.find(entity_id)->stats.current_derived.move_speed;
        const auto haste = mmo::core::status::make_instance(
            mmo::core::status::make_haste_definition(),
            epoch());
        test::require(world.entities.apply_status(entity_id, haste), "haste should apply");
        test::require(
            world.entities.find(entity_id)->stats.current_derived.move_speed > base_move_speed,
            "haste should affect derived stats");

        const auto before = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 119, epoch() + mmo::core::time::Milliseconds{ 5999 } });
        const auto at_deadline = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 120, epoch() + mmo::core::time::Milliseconds{ 6000 } });

        test::require_equal(static_cast<std::uint64_t>(0), before.status_changes, "status before expiration");
        test::require_equal(static_cast<std::uint64_t>(1), at_deadline.status_changes, "status expiration change");
        test::require(
            world.entities.find(entity_id)->combat.status_effects.find(mmo::core::status::Kind::haste) == nullptr,
            "haste should expire at deadline");
        test::require_equal(
            base_move_speed,
            world.entities.find(entity_id)->stats.current_derived.move_speed,
            "derived stats after expiration");
        details = "before_deadline=active at_deadline=expired";
    }

    auto test_canonical_entity_traversal(std::string& details) -> void
    {
        mmo::core::runtime::World first{};
        mmo::core::runtime::World second{};
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

        const auto first_stats = mmo::world::step(first, items, mmo::world::TickContext{ 42, epoch() });
        const auto second_stats = mmo::world::step(second, items, mmo::world::TickContext{ 42, epoch() });
        require_equal_stats(first_stats, second_stats, "canonical traversal");
        details = "insertions=30,10,20/20,30,10 traversal=10,20,30";
    }

    auto build_repeated_world(const std::vector<mmo::core::id::EntityId>& ids)
        -> mmo::core::runtime::World
    {
        mmo::core::runtime::World world{};
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
        test::require(world.scheduler.try_schedule(due), "repeated due event should schedule");

        auto future = due;
        future.due_at = epoch() + mmo::core::time::Milliseconds{ 10000 };
        future.counter = 2;
        test::require(world.scheduler.try_schedule(future), "repeated future event should schedule");
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
            const auto first_tick = mmo::world::step(first, items, context);
            const auto second_tick = mmo::world::step(second, items, context);
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
        test::require_equal(static_cast<std::size_t>(1), first.scheduler.size(), "future event remains");
        details = "worlds=2 entities_per_world=128 ticks=100 entity_considerations=25600";
    }
}

int main()
{
    std::vector<test::TestResult> results;
    results.push_back(test::run_test("simulation.empty_tick", test_empty_tick));
    results.push_back(test::run_test("simulation.due_event_exactly_once", test_due_event_exactly_once));
    results.push_back(test::run_test("simulation.future_event_pending", test_future_event_remains_pending));
    results.push_back(test::run_test("simulation.dirty_load_once", test_dirty_load_recalculates_once));
    results.push_back(test::run_test("simulation.periodic_cadence", test_periodic_status_cadence));
    results.push_back(test::run_test("simulation.periodic_resource_invariants", test_periodic_resource_invariants));
    results.push_back(test::run_test("simulation.status_expiration", test_status_expiration_boundary));
    results.push_back(test::run_test("simulation.canonical_traversal", test_canonical_entity_traversal));
    results.push_back(test::run_test("simulation.repeated_identical_worlds", test_repeated_simulation_is_identical));
    return test::report_results("Simulation integration", results, std::cout);
}
