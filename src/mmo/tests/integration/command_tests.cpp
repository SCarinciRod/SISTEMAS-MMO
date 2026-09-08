#include "mmo/tests/support/test.hpp"
#include "mmo/world/world.hpp"
#include "mmo/server/loop.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace test = mmo::tests::support;
    namespace command = mmo::world::command;

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
        blueprint.display_name = "Command Test Player";
        blueprint.base_primary = mmo::core::stat::Primary{ 12, 12, 8, 6, 5, 7, 4 };
        blueprint.level = 1;
        return blueprint;
    }

    auto spawn_player(
        mmo::world::World& world,
        mmo::core::id::EntityId entity_id,
        mmo::core::id::ZoneId zone_id = static_cast<mmo::core::id::ZoneId>(1)) -> void
    {
        test::require(
            world.spawn_entity(entity_id, make_blueprint(), zone_id, epoch()),
            "command test player should spawn");
    }

    auto make_move(
        mmo::core::time::TickCount target_tick,
        std::uint64_t sequence,
        mmo::core::id::EntityId actor,
        mmo::core::id::ZoneId destination_zone_id) -> command::Envelope
    {
        return command::Envelope{
            target_tick,
            command::Sequence{ sequence },
            actor,
            command::MoveToZone{ destination_zone_id }
        };
    }

    auto require_push_accepted(
        command::Inbox& inbox,
        command::Envelope envelope,
        mmo::core::time::TickCount current_tick,
        std::string_view label) -> void
    {
        const auto result = inbox.try_push(std::move(envelope), current_tick);
        test::require_equal(command::IngressError::none, result.error, label);
        test::require(result.accepted(), "accepted ingress result should report accepted");
    }

    auto require_actor_zone(
        const mmo::world::World& world,
        mmo::core::id::EntityId actor,
        mmo::core::id::ZoneId expected_zone,
        std::string_view label) -> void
    {
        const auto* record = world.find_entity(actor);
        test::require(record != nullptr, "command actor should exist");
        test::require_equal(expected_zone, record->placement.zone_id(), label);
    }

    auto require_equal_results(
        const mmo::world::TickStats& expected,
        const mmo::world::TickStats& actual,
        std::string_view label) -> void
    {
        test::require_equal(
            expected.commands_received,
            actual.commands_received,
            std::string(label).append(" received"));
        test::require_equal(
            expected.commands_processed,
            actual.commands_processed,
            std::string(label).append(" processed"));
        test::require_equal(
            expected.commands_accepted,
            actual.commands_accepted,
            std::string(label).append(" accepted"));
        test::require_equal(
            expected.commands_rejected,
            actual.commands_rejected,
            std::string(label).append(" rejected"));
        test::require_equal(
            expected.command_results.size(),
            actual.command_results.size(),
            std::string(label).append(" result count"));

        for (std::size_t index = 0; index < expected.command_results.size(); ++index)
        {
            test::require_equal(
                expected.command_results[index].sequence.value,
                actual.command_results[index].sequence.value,
                std::string(label).append(" result sequence"));
            test::require_equal(
                expected.command_results[index].reason,
                actual.command_results[index].reason,
                std::string(label).append(" result reason"));
        }
    }

    auto require_equivalent_worlds(
        const mmo::world::World& expected,
        const mmo::world::World& actual,
        mmo::core::id::EntityId actor,
        mmo::core::id::ZoneId maximum_zone) -> void
    {
        require_actor_zone(
            actual,
            actor,
            expected.find_entity(actor)->placement.zone_id(),
            "equivalent actor zone");
        test::require(
            expected.active_zone_ids() == actual.active_zone_ids(),
            "equivalent worlds should have the same active zones");

        for (mmo::core::id::ZoneId zone_id = 1; zone_id <= maximum_zone; ++zone_id)
        {
            const auto* expected_zone = expected.find_zone(zone_id);
            const auto* actual_zone = actual.find_zone(zone_id);
            test::require(
                (expected_zone == nullptr) == (actual_zone == nullptr),
                "equivalent worlds should have the same zones");
            if (expected_zone == nullptr)
            {
                continue;
            }

            test::require_equal(
                expected_zone->entity_count,
                actual_zone->entity_count,
                "equivalent zone entity count");
            test::require_equal(
                expected_zone->player_count,
                actual_zone->player_count,
                "equivalent zone player count");
            test::require_equal(
                expected_zone->last_tick,
                actual_zone->last_tick,
                "equivalent zone last tick");
        }

        test::require(
            expected.has_consistent_spatial_state() && actual.has_consistent_spatial_state(),
            "equivalent worlds should preserve spatial invariants");
    }

    auto test_resource_and_inventory(std::string& details) -> void
    {
        mmo::world::World world;
        spawn_player(world, 1);
        mmo::core::item::Catalog items;
        mmo::core::item::Definition definition{};
        definition.identity.item_template_id = 1;
        test::require(items.insert(definition), "item definition");
        const auto health = world.find_entity(1)->resources.health_current;
        command::Inbox inbox;
        require_push_accepted(inbox, { 0, { 2 }, 1, command::AddInventoryItem{ 100, 1 } }, 0, "grant");
        require_push_accepted(inbox, { 0, { 1 }, 1, command::AdjustHealth{ -1 } }, 0, "health");
        test::require_equal(command::IngressError::duplicate_sequence,
            inbox.try_push({ 0, { 2 }, 1, command::AddInventoryItem{ 100, 1 } }, 0).error, "duplicate grant");
        const auto capture = inbox.capture_for_tick(0);
        const auto stats = mmo::world::step(world, items, { 0, epoch() }, capture.batch);
        test::require_equal(std::uint64_t{ 2 }, stats.commands_accepted, "two accepted");
        test::require_equal(health - 1, world.find_entity(1)->resources.health_current, "health delta");
        test::require_equal(std::size_t{ 1 }, world.find_entity(1)->inventory.items.size(), "one item");
        test::require_equal(std::size_t{ 2 }, stats.domain_events.size(), "facts");
        test::require(std::holds_alternative<mmo::world::domain::HealthAdjusted>(stats.domain_events[0].payload), "health fact");
        test::require(std::holds_alternative<mmo::world::domain::InventoryItemAdded>(stats.domain_events[1].payload), "inventory fact");
        require_push_accepted(inbox, { 1, { 3 }, 1, command::AdjustHealth{} }, 1, "zero delta ingress");
        require_push_accepted(inbox, { 1, { 4 }, 1, command::AddInventoryItem{ 101, 999 } }, 1, "bad template ingress");
        require_push_accepted(inbox, { 1, { 5 }, 1, command::AddInventoryItem{ 100, 1 } }, 1, "duplicate item ingress");
        const auto invalid = inbox.capture_for_tick(1);
        const auto rejected = mmo::world::step(world, items, { 1, epoch() }, invalid.batch);
        test::require_equal(std::uint64_t{ 3 }, rejected.commands_rejected, "three rejections");
        test::require(rejected.domain_events.empty(), "rejections are not facts");
        test::require_equal(health - 1, world.find_entity(1)->resources.health_current, "unchanged health");
        test::require_equal(std::size_t{ 1 }, world.find_entity(1)->inventory.items.size(), "unchanged inventory");
        test::require(world.has_consistent_spatial_state(), "consistent state");
        details = "health and inventory accepted; duplicate and invalid input rejected";
    }

    auto test_fault_stops_loop(std::string& details) -> void
    {
        class FailingObserver final : public mmo::world::TickObserver
        {
        public:
            auto phase_started(mmo::world::TickPhase) -> void override {}
            auto phase_finished(mmo::world::TickPhase phase) -> void override
            {
                if (phase == mmo::world::TickPhase::authoritative_commands)
                    throw std::runtime_error("injected failure after mutation");
            }
        } observer;
        mmo::world::World world;
        spawn_player(world, 1);
        const mmo::core::item::Catalog items;
        command::Inbox inbox;
        require_push_accepted(inbox, make_move(0, 1, 1, 2), 0, "first tick");
        require_push_accepted(inbox, make_move(1, 2, 1, 3), 0, "next tick");
        mmo::server::LoopConfig config{};
        config.sleep = false;
        config.max_ticks = 2;
        mmo::core::log::Logger logger;
        const auto stats = mmo::server::run_loop(world, items, inbox, config, logger, &observer);
        test::require(stats.faulted && world.is_faulted(), "fault marked");
        test::require_equal(std::uint64_t{ 0 }, stats.ticks, "failed tick not completed");
        require_actor_zone(world, 1, 2, "second tick not executed");
        test::require_equal(std::size_t{ 1 }, inbox.size(), "future input retained");
        const auto restarted = mmo::server::run_loop(world, items, inbox, config, logger);
        test::require(restarted.faulted && restarted.ticks == 0, "restart prevented");
        bool threw = false;
        try { (void)mmo::world::step(world, items, { 1, epoch() }, command::Batch{ 1 }); }
        catch (const std::logic_error&) { threw = true; }
        test::require(threw, "direct step prevented");
        details = "partial tick faulted; loop stopped; restart rejected";
    }

    auto test_empty_batch(std::string& details) -> void
    {
        mmo::world::World world;
        const mmo::core::item::Catalog items;
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 7, epoch() },
            command::Batch{ 7 });

        test::require_equal(std::uint64_t{ 0 }, stats.commands_received, "empty received");
        test::require_equal(std::uint64_t{ 0 }, stats.commands_processed, "empty processed");
        test::require_equal(std::uint64_t{ 0 }, stats.commands_accepted, "empty accepted");
        test::require_equal(std::uint64_t{ 0 }, stats.commands_rejected, "empty rejected");
        test::require(stats.command_results.empty(), "empty batch should have no results");
        test::require(world.has_consistent_spatial_state(), "empty tick spatial state");
        details = "batch=0 processed=0";
    }

    auto test_accepted_move(std::string& details) -> void
    {
        constexpr mmo::core::id::EntityId actor = 41;
        constexpr mmo::core::id::ZoneId destination = 2;
        mmo::world::World world;
        mmo::core::item::Catalog items;
        command::Inbox inbox;
        spawn_player(world, actor);
        require_push_accepted(inbox, make_move(5, 1, actor, destination), 5, "move ingress");

        const auto capture = inbox.capture_for_tick(5);
        test::require(capture.rejections.empty(), "valid move capture should not reject");
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 5, epoch() },
            capture.batch);

        test::require_equal(std::uint64_t{ 1 }, stats.commands_received, "move received");
        test::require_equal(std::uint64_t{ 1 }, stats.commands_processed, "move processed");
        test::require_equal(std::uint64_t{ 1 }, stats.commands_accepted, "move accepted");
        test::require_equal(std::uint64_t{ 0 }, stats.commands_rejected, "move rejected");
        test::require(stats.command_results.front().accepted(), "move result should be accepted");
        require_actor_zone(world, actor, destination, "accepted move destination");
        test::require(world.has_consistent_spatial_state(), "accepted move spatial state");
        details = "accepted=1 actor_zone=2";
    }

    auto test_canonical_sequence_order(std::string& details) -> void
    {
        constexpr mmo::core::id::EntityId actor = 42;
        mmo::world::World world;
        mmo::core::item::Catalog items;
        command::Inbox inbox;
        spawn_player(world, actor);

        require_push_accepted(inbox, make_move(9, 30, actor, 4), 9, "sequence 30 ingress");
        require_push_accepted(inbox, make_move(9, 10, actor, 2), 9, "sequence 10 ingress");
        require_push_accepted(inbox, make_move(9, 20, actor, 3), 9, "sequence 20 ingress");

        const auto capture = inbox.capture_for_tick(9);
        test::require_equal(std::size_t{ 3 }, capture.batch.size(), "canonical batch size");
        test::require_equal(std::uint64_t{ 10 }, capture.batch[0].sequence.value, "first sequence");
        test::require_equal(std::uint64_t{ 20 }, capture.batch[1].sequence.value, "second sequence");
        test::require_equal(std::uint64_t{ 30 }, capture.batch[2].sequence.value, "third sequence");

        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 9, epoch() },
            capture.batch);
        test::require_equal(std::uint64_t{ 3 }, stats.commands_accepted, "canonical accepted");
        test::require_equal(std::uint64_t{ 10 }, stats.command_results[0].sequence.value, "first result");
        test::require_equal(std::uint64_t{ 20 }, stats.command_results[1].sequence.value, "second result");
        test::require_equal(std::uint64_t{ 30 }, stats.command_results[2].sequence.value, "third result");
        require_actor_zone(world, actor, 4, "canonical final destination");
        test::require(world.has_consistent_spatial_state(), "canonical move spatial state");
        details = "ingress=30,10,20 processed=10,20,30";
    }

    auto test_ingress_order_independence(std::string& details) -> void
    {
        constexpr mmo::core::id::EntityId actor = 43;
        mmo::world::World first;
        mmo::world::World second;
        mmo::core::item::Catalog items;
        command::Inbox first_inbox;
        command::Inbox second_inbox;
        spawn_player(first, actor);
        spawn_player(second, actor);

        const std::vector<command::Envelope> commands{
            make_move(3, 100, actor, 2),
            make_move(3, 200, actor, 3),
            make_move(3, 300, actor, 4)
        };
        require_push_accepted(first_inbox, commands[2], 3, "first order sequence 300");
        require_push_accepted(first_inbox, commands[0], 3, "first order sequence 100");
        require_push_accepted(first_inbox, commands[1], 3, "first order sequence 200");
        require_push_accepted(second_inbox, commands[1], 3, "second order sequence 200");
        require_push_accepted(second_inbox, commands[2], 3, "second order sequence 300");
        require_push_accepted(second_inbox, commands[0], 3, "second order sequence 100");

        const auto first_capture = first_inbox.capture_for_tick(3);
        const auto second_capture = second_inbox.capture_for_tick(3);
        const auto context = mmo::world::TickContext{ 3, epoch() };
        const auto first_stats = mmo::world::step(first, items, context, first_capture.batch);
        const auto second_stats = mmo::world::step(second, items, context, second_capture.batch);

        require_equal_results(first_stats, second_stats, "different ingress order");
        require_equivalent_worlds(first, second, actor, 4);
        details = "same_set=3 final_zone=4";
    }

    auto test_bounded_capacity_and_metrics(std::string& details) -> void
    {
        command::Inbox inbox(command::InboxConfig{ 2, 0 });
        require_push_accepted(inbox, make_move(12, 1, 1, 2), 12, "capacity first");
        require_push_accepted(inbox, make_move(12, 2, 1, 3), 12, "capacity second");
        const auto rejected = inbox.try_push(make_move(12, 3, 1, 4), 12);
        test::require_equal(command::IngressError::queue_full, rejected.error, "capacity error");

        const auto metrics = inbox.metrics();
        test::require_equal(std::size_t{ 2 }, metrics.depth, "capacity depth");
        test::require_equal(std::size_t{ 2 }, metrics.capacity, "capacity limit");
        test::require_equal(std::uint64_t{ 1 }, metrics.rejected_full, "capacity rejected full");
        test::require_equal(std::size_t{ 2 }, metrics.high_watermark, "capacity high watermark");
        const auto capture = inbox.capture_for_tick(12);
        test::require_equal(std::size_t{ 2 }, capture.batch.size(), "capacity preserved batch");
        test::require_equal(std::uint64_t{ 1 }, capture.batch[0].sequence.value, "capacity first sequence");
        test::require_equal(std::uint64_t{ 2 }, capture.batch[1].sequence.value, "capacity second sequence");
        details = "capacity=2 accepted=2 rejected_full=1";
    }

    auto test_old_command_rejected(std::string& details) -> void
    {
        command::Inbox inbox(command::InboxConfig{ 4, 4 });
        const auto result = inbox.try_push(make_move(19, 1, 1, 2), 20);
        test::require_equal(command::IngressError::too_old, result.error, "old command error");
        test::require_equal(std::size_t{ 0 }, inbox.size(), "old command depth");
        details = "target=19 current=20 error=too_old";
    }

    auto test_future_window_boundary(std::string& details) -> void
    {
        command::Inbox inbox(command::InboxConfig{ 4, 3 });
        require_push_accepted(inbox, make_move(23, 1, 1, 2), 20, "future boundary");
        const auto too_far = inbox.try_push(make_move(24, 2, 1, 3), 20);
        test::require_equal(
            command::IngressError::too_far_in_future,
            too_far.error,
            "future overflow");
        test::require_equal(std::size_t{ 1 }, inbox.size(), "future retained depth");
        details = "maximum_future=3 boundary=accepted plus_one=rejected";
    }

    auto test_duplicate_sequence_window(std::string& details) -> void
    {
        command::Inbox inbox(command::InboxConfig{ 2, 0 });
        require_push_accepted(inbox, make_move(30, 1, 1, 2), 30, "dedupe first");
        const auto pending_duplicate = inbox.try_push(make_move(30, 1, 1, 3), 30);
        test::require_equal(
            command::IngressError::duplicate_sequence,
            pending_duplicate.error,
            "pending duplicate");

        const auto first_capture = inbox.capture_for_tick(30);
        test::require_equal(std::size_t{ 1 }, first_capture.batch.size(), "first capture size");
        const auto recent_duplicate = inbox.try_push(make_move(30, 1, 1, 3), 30);
        test::require_equal(
            command::IngressError::duplicate_sequence,
            recent_duplicate.error,
            "recent duplicate");

        require_push_accepted(inbox, make_move(30, 2, 1, 2), 30, "dedupe second");
        const auto second_capture = inbox.capture_for_tick(30);
        test::require_equal(std::size_t{ 1 }, second_capture.batch.size(), "second capture size");
        require_push_accepted(inbox, make_move(30, 3, 1, 2), 30, "dedupe third");
        const auto third_capture = inbox.capture_for_tick(30);
        test::require_equal(std::size_t{ 1 }, third_capture.batch.size(), "third capture size");

        require_push_accepted(
            inbox,
            make_move(30, 1, 1, 4),
            30,
            "evicted sequence should leave bounded history");
        details = "history_capacity=2 pending_and_recent_duplicates=rejected oldest=evicted";
    }

    auto test_closed_batch(std::string& details) -> void
    {
        command::Inbox inbox(command::InboxConfig{ 4, 0 });
        require_push_accepted(inbox, make_move(40, 1, 1, 2), 40, "closed first");
        const auto first_capture = inbox.capture_for_tick(40);
        require_push_accepted(inbox, make_move(40, 2, 1, 3), 40, "closed second");

        test::require_equal(std::size_t{ 1 }, first_capture.batch.size(), "closed batch size");
        test::require_equal(
            std::uint64_t{ 1 },
            first_capture.batch[0].sequence.value,
            "closed batch sequence");
        const auto second_capture = inbox.capture_for_tick(40);
        test::require_equal(std::size_t{ 1 }, second_capture.batch.size(), "next batch size");
        test::require_equal(
            std::uint64_t{ 2 },
            second_capture.batch[0].sequence.value,
            "next batch sequence");
        details = "captured_batch=1 later_ingress_isolated=1";
    }

    auto test_gameplay_rejections(std::string& details) -> void
    {
        constexpr mmo::core::id::EntityId actor = 44;
        mmo::world::World world;
        mmo::core::item::Catalog items;
        command::Inbox inbox(command::InboxConfig{ 4, 0 });
        spawn_player(world, actor);

        require_push_accepted(inbox, make_move(50, 2, 999, 2), 50, "missing actor ingress");
        require_push_accepted(
            inbox,
            make_move(50, 3, actor, mmo::core::id::invalid_zone_id),
            50,
            "invalid destination ingress");
        require_push_accepted(inbox, make_move(50, 4, actor, 1), 50, "same destination ingress");
        const auto capture = inbox.capture_for_tick(50);
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ 50, epoch() },
            capture.batch);

        test::require_equal(std::uint64_t{ 3 }, stats.commands_received, "invalid received");
        test::require_equal(std::uint64_t{ 3 }, stats.commands_processed, "invalid processed");
        test::require_equal(std::uint64_t{ 0 }, stats.commands_accepted, "invalid accepted");
        test::require_equal(std::uint64_t{ 3 }, stats.commands_rejected, "invalid rejected");
        test::require_equal(
            command::ExecutionRejection::entity_missing,
            stats.command_results[0].reason,
            "missing actor reason");
        test::require_equal(
            command::ExecutionRejection::invalid_destination,
            stats.command_results[1].reason,
            "invalid destination reason");
        test::require_equal(
            command::ExecutionRejection::already_in_destination,
            stats.command_results[2].reason,
            "same destination reason");
        require_actor_zone(world, actor, 1, "rejected commands preserve actor zone");
        test::require(world.has_consistent_spatial_state(), "rejected command spatial state");
        details = "processed=3 entity_missing=1 invalid_destination=1 already_there=1";
    }

    auto test_scheduled_event_precedes_commands(std::string& details) -> void
    {
        constexpr mmo::core::id::EntityId actor = 46;
        constexpr mmo::core::time::TickCount tick = 80;
        mmo::world::World world;
        mmo::core::item::Catalog items;
        command::Inbox inbox(command::InboxConfig{ 2, 0 });
        spawn_player(world, actor, 1);

        mmo::core::event::Event migration{};
        migration.type = mmo::core::event::Type::migration_completed;
        migration.due_at = epoch();
        migration.entity_id = actor;
        migration.zone_id = 2;
        test::require(world.try_schedule_event(migration), "phase-order migration should schedule");
        require_push_accepted(inbox, make_move(tick, 1, actor, 3), tick, "phase-order command");

        const auto capture = inbox.capture_for_tick(tick);
        const auto stats = mmo::world::step(
            world,
            items,
            mmo::world::TickContext{ tick, epoch() },
            capture.batch);

        test::require_equal(std::uint64_t{ 1 }, stats.events_applied, "phase-order event applied");
        test::require_equal(std::uint64_t{ 1 }, stats.commands_accepted, "phase-order command accepted");
        require_actor_zone(world, actor, 3, "command should run after due migration");
        test::require(world.has_consistent_spatial_state(), "phase-order spatial state");
        details = "scheduled_migration=zone2 then_command=zone3";
    }

    auto test_repeated_deterministic_streams(std::string& details) -> void
    {
        constexpr mmo::core::id::EntityId actor = 45;
        constexpr mmo::core::time::TickCount tick_count = 32;
        mmo::world::World first;
        mmo::world::World second;
        mmo::core::item::Catalog items;
        command::Inbox first_inbox(command::InboxConfig{ 16, 0 });
        command::Inbox second_inbox(command::InboxConfig{ 16, 0 });
        spawn_player(first, actor);
        spawn_player(second, actor);

        std::uint64_t total_accepted = 0;
        for (mmo::core::time::TickCount tick = 0; tick < tick_count; ++tick)
        {
            const auto base = (tick * 3) + 1;
            const auto first_move = make_move(tick, base, actor, 2);
            const auto second_move = make_move(tick, base + 1, actor, 3);
            const auto third_move = make_move(tick, base + 2, actor, 1);

            require_push_accepted(first_inbox, third_move, tick, "stream first third");
            require_push_accepted(first_inbox, first_move, tick, "stream first first");
            require_push_accepted(first_inbox, second_move, tick, "stream first second");
            require_push_accepted(second_inbox, second_move, tick, "stream second second");
            require_push_accepted(second_inbox, third_move, tick, "stream second third");
            require_push_accepted(second_inbox, first_move, tick, "stream second first");

            const auto first_capture = first_inbox.capture_for_tick(tick);
            const auto second_capture = second_inbox.capture_for_tick(tick);
            const auto simulation_time = epoch() + mmo::core::time::Milliseconds{
                static_cast<mmo::core::time::Milliseconds::rep>(tick * 50)
            };
            const auto context = mmo::world::TickContext{ tick, simulation_time };
            const auto first_stats = mmo::world::step(
                first,
                items,
                context,
                first_capture.batch);
            const auto second_stats = mmo::world::step(
                second,
                items,
                context,
                second_capture.batch);

            require_equal_results(first_stats, second_stats, "repeated stream");
            test::require_equal(std::uint64_t{ 3 }, first_stats.commands_accepted, "stream accepted");
            require_equivalent_worlds(first, second, actor, 3);
            total_accepted += first_stats.commands_accepted;
        }

        test::require_equal(std::uint64_t{ 96 }, total_accepted, "stream total accepted");
        require_actor_zone(first, actor, 1, "stream final zone");
        details = "ticks=32 commands=96 deterministic_worlds=2";
    }

    auto test_invalid_ingress_identity(std::string& details) -> void
    {
        command::Inbox inbox(command::InboxConfig{ 4, 0 });
        const auto invalid_sequence = inbox.try_push(make_move(60, 0, 1, 2), 60);
        test::require_equal(
            command::IngressError::invalid_sequence,
            invalid_sequence.error,
            "invalid sequence error");
        const auto invalid_actor = inbox.try_push(
            make_move(60, 1, mmo::core::id::invalid_entity_id, 2),
            60);
        test::require_equal(
            command::IngressError::invalid_actor,
            invalid_actor.error,
            "invalid actor error");
        test::require_equal(std::size_t{ 0 }, inbox.size(), "invalid ingress depth");
        details = "invalid_sequence=1 invalid_actor=1 queued=0";
    }

    auto test_expired_capture_metrics(std::string& details) -> void
    {
        command::Inbox inbox(command::InboxConfig{ 4, 2 });
        require_push_accepted(inbox, make_move(70, 7, 1, 2), 70, "expiring ingress");
        const auto capture = inbox.capture_for_tick(71);

        test::require(capture.batch.empty(), "expired command should not enter batch");
        test::require_equal(std::size_t{ 1 }, capture.rejections.size(), "expired rejection count");
        test::require_equal(
            command::IngressError::too_old,
            capture.rejections[0].error,
            "expired rejection reason");
        const auto metrics = inbox.metrics();
        test::require_equal(std::size_t{ 0 }, metrics.depth, "expired depth");
        test::require_equal(
            std::uint64_t{ 1 },
            metrics.expired_before_capture,
            "expired metric");

        const auto duplicate = inbox.try_push(make_move(71, 7, 1, 3), 71);
        test::require_equal(
            command::IngressError::duplicate_sequence,
            duplicate.error,
            "expired sequence history");
        details = "expired=1 surfaced=1 remembered_for_dedupe=1";
    }
}

int main()
{
    std::vector<test::TestResult> results;
    results.push_back(test::run_test("command.resource_inventory", test_resource_and_inventory));
    results.push_back(test::run_test("command.fault_stops_loop", test_fault_stops_loop));
    results.push_back(test::run_test("command.empty_batch", test_empty_batch));
    results.push_back(test::run_test("command.accepted_move", test_accepted_move));
    results.push_back(test::run_test("command.canonical_sequence", test_canonical_sequence_order));
    results.push_back(test::run_test("command.ingress_order_independence", test_ingress_order_independence));
    results.push_back(test::run_test("command.bounded_capacity", test_bounded_capacity_and_metrics));
    results.push_back(test::run_test("command.old_rejected", test_old_command_rejected));
    results.push_back(test::run_test("command.future_window", test_future_window_boundary));
    results.push_back(test::run_test("command.duplicate_window", test_duplicate_sequence_window));
    results.push_back(test::run_test("command.closed_batch", test_closed_batch));
    results.push_back(test::run_test("command.gameplay_rejections", test_gameplay_rejections));
    results.push_back(test::run_test("command.tick_phase_order", test_scheduled_event_precedes_commands));
    results.push_back(test::run_test("command.repeated_streams", test_repeated_deterministic_streams));
    results.push_back(test::run_test("command.invalid_ingress_identity", test_invalid_ingress_identity));
    results.push_back(test::run_test("command.expired_capture_metrics", test_expired_capture_metrics));
    return test::report_results("Command pipeline integration", results, std::cout);
}
