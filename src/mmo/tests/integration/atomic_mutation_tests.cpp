#include <iostream>
#include <new>
#include <sstream>
#include "mmo/tests/support/test.hpp"
#include "mmo/tests/support/allocation_failure.hpp"
#include "mmo/world/world.hpp"

namespace mmo::tests::support
{
    struct WorldMutationAccess
    {
        using Point = world::World::MutationPoint;
        static auto hook(world::World& value, void (*callback)(Point)) -> void
        {
            value.mutation_hook_ = callback;
        }
        static auto zones(const world::World& value) { return value.zones_.list_ids(); }
        static auto scheduler(const world::World& value) { return value.scheduler_; }
    };
}

namespace
{
    namespace t = mmo::tests::support;
    namespace w = mmo::world;
    namespace c = w::command;
    namespace s = w::scheduled;
    namespace d = w::domain;
    namespace core = mmo::core;
    using Access = t::WorldMutationAccess;
    using Point = Access::Point;
    const core::time::TimePoint epoch{};
    Point failure_point = Point::zone_prepared;

    auto inject(Point point) -> void
    {
        if (point == failure_point) throw std::bad_alloc{};
    }
    auto blueprint(bool player = true) -> core::entity::Blueprint
    {
        core::entity::Blueprint result{};
        result.template_id = 1;
        result.species_id = 1;
        result.type = player ? core::entity::Type::player : core::entity::Type::npc;
        result.base_primary = { 12, 12, 8, 6, 5, 7, 4 };
        return result;
    }
    auto spawn(w::World& world, std::uint64_t id = 1, std::uint64_t zone = 1, bool player = true) -> void
    {
        t::require(world.spawn_entity(id, blueprint(player), zone, epoch), "spawn fixture");
    }
    auto schedule(w::World& world, std::uint64_t id, s::Payload payload) -> void
    {
        t::require(world.try_schedule_action({ { id }, epoch, payload }).accepted(), "schedule fixture");
    }
    auto step(w::World& world) -> w::TickStats
    {
        return w::step(world, {}, { 0, epoch }, c::Batch{ 0 });
    }
    auto command(w::World& world, c::Payload payload) -> w::TickStats
    {
        c::Inbox inbox;
        t::require(inbox.try_push({ 0, { 1 }, 1, payload }, 0).accepted(), "command fixture");
        return w::step(world, {}, { 0, epoch }, inbox.capture_for_tick(0).batch);
    }

    // Logical comparison includes zone existence, timestamps, both membership indexes and pending work.
    auto snapshot(const w::World& world) -> std::string
    {
        std::ostringstream out;
        for (const auto id : world.list_entity_ids())
        {
            const auto& e = *world.find_entity(id);
            out << "E" << id << ':' << e.placement.zone_id() << ':' << static_cast<int>(e.identity.type)
                << ':' << e.resources.health_current << ':' << e.resources.mana_current
                << ':' << e.load.carried_weight << ':' << e.load.inventory_dirty
                << ':' << e.stats.current_derived.move_speed;
            for (const auto& item : e.inventory.items)
                out << "I" << item.item_id << ':' << item.item_template_id << ':' << item.quantity;
        }
        for (const auto id : Access::zones(world))
        {
            const auto& z = *world.find_zone(id);
            out << "Z" << id << ':' << z.entity_count << ':' << z.player_count << ':' << z.wake_requested
                << ':' << z.last_tick << ':' << z.last_player_activity.time_since_epoch().count();
            for (const auto entity : world.entity_ids_in_zone(id)) out << "M" << entity;
        }
        for (const auto id : world.active_zone_ids()) out << "A" << id;
        auto scheduler = Access::scheduler(world);
        while (const auto* action = scheduler.peek_ready(core::time::TimePoint::max()))
        {
            out << "S" << action->id.value << ':' << action->due_at.time_since_epoch().count()
                << ':' << action->payload.index();
            std::visit([&](const auto& payload)
            {
                using T = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<T, s::CompleteMigration>) out << ':' << payload.entity_id << ':' << payload.destination_zone_id;
                else if constexpr (std::is_same_v<T, s::EvolutionDue>) out << ':' << payload.entity_id;
                else if constexpr (std::is_same_v<T, s::RegionNotice>) out << ':' << payload.zone_id << ':' << payload.notice_id;
                else out << ':' << payload.zone_id;
            }, action->payload);
            scheduler.acknowledge_ready();
        }
        return out.str();
    }
    auto output(const w::TickStats& stats) -> std::string
    {
        std::ostringstream out;
        for (const auto& result : stats.command_results) out << "C" << result.sequence.value << ':' << static_cast<int>(result.reason);
        for (const auto& result : stats.scheduled_results) out << "S" << result.id.value << ':' << static_cast<int>(result.reason);
        for (const auto& event : stats.domain_events)
        {
            out << "F" << event.tick << ':' << event.sequence << ':' << event.occurred_at.time_since_epoch().count()
                << ':' << event.cause.index() << ':' << std::visit([](const auto& cause) { return cause.id.value; }, event.cause)
                << ':' << event.payload.index();
            std::visit([&](const auto& payload)
            {
                using T = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<T, d::EntityMoved>) out << ':' << payload.entity_id << ':' << payload.source_zone_id << ':' << payload.destination_zone_id;
                else if constexpr (std::is_same_v<T, d::HealthAdjusted>) out << ':' << payload.entity_id << ':' << payload.previous_health << ':' << payload.current_health;
                else if constexpr (std::is_same_v<T, d::InventoryItemAdded>) out << ':' << payload.entity_id << ':' << payload.template_id << ':' << payload.quantity;
                else if constexpr (std::is_same_v<T, d::RegionNoticeEmitted>) out << ':' << payload.zone_id << ':' << payload.notice_id;
                else out << ':' << payload.zone_id;
            }, event.payload);
        }
        return out.str();
    }
    auto assert_faulted(w::World& world) -> void
    {
        t::require(world.is_faulted(), "unexpected failure faults World");
        bool blocked = false;
        try { (void)step(world); } catch (const std::logic_error&) { blocked = true; }
        t::require(blocked, "faulted tick cannot resume");
        blocked = false;
        try { (void)world.wake_zone(50); } catch (const std::logic_error&) { blocked = true; }
        t::require(blocked, "faulted direct mutation cannot resume");
        t::require(world.has_consistent_spatial_state(), "spatial invariants after failure");
    }

    auto move_success(std::string&) -> void
    {
        w::World world;
        spawn(world); spawn(world, 2, 1, false); spawn(world, 3, 2, false);
        const auto result = command(world, c::MoveToZone{ 2 });
        t::require(result.command_results[0].accepted(), "move accepted");
        t::require_equal(std::size_t{ 1 }, result.domain_events.size(), "one fact after move");
        t::require_equal(std::uint64_t{ 2 }, world.find_entity(1)->placement.zone_id(), "placement");
        t::require(world.find_zone(1)->entity_count == 1 && world.find_zone(1)->player_count == 0, "source counts");
        t::require(world.find_zone(2)->entity_count == 2 && world.find_zone(2)->player_count == 1, "destination counts");
        t::require(world.active_zone_ids() == std::set<core::id::ZoneId>{ 2 }, "active index");
        t::require(world.has_consistent_spatial_state(), "move invariants");
    }
    auto move_rejected(std::string&) -> void
    {
        w::World world;
        spawn(world);
        (void)step(world); // Settle initial load maintenance before comparing a rejected tick.
        t::require(world.try_schedule_action({ { 7 }, epoch + core::time::Milliseconds{ 10 }, s::WakeZone{ 9 } }).accepted(), "future work");
        const auto before = snapshot(world);
        t::require(!world.move_entity_to_zone(1, 0, epoch), "invalid destination");
        t::require(!world.move_entity_to_zone(99, 2, epoch), "missing actor");
        const auto stats = command(world, c::MoveToZone{ 0 });
        t::require(!stats.command_results[0].accepted() && stats.domain_events.empty(), "rejected command has no fact");
        t::require_equal(before, snapshot(world), "rejection preserves all logical state");
        t::require(!world.is_faulted(), "expected rejection does not fault");
    }
    auto spawn_erase(std::string&) -> void
    {
        w::World world;
        spawn(world);
        auto before = snapshot(world);
        t::require(!world.spawn_entity(1, blueprint(), 99, epoch), "duplicate spawn");
        t::require(!world.spawn_entity(0, blueprint(), 99, epoch), "invalid spawn");
        t::require(!world.spawn_entity(2, blueprint(), 0, epoch), "invalid zone");
        t::require(!world.erase_entity(99, epoch), "missing erase");
        t::require_equal(before, snapshot(world), "spawn/erase rejection unchanged");
        spawn(world, 2, 2);
        t::require(world.has_consistent_spatial_state(), "spawn invariants");
        t::require(world.erase_entity(2, epoch), "erase succeeds");
        t::require(!world.find_entity(2) && world.find_zone(2)->entity_count == 0 && !world.should_tick_full(2), "no ghost after erase");
        t::require(world.has_consistent_spatial_state(), "erase invariants");
    }

    auto injected_preparation(std::string&) -> void
    {
        for (int operation = 0; operation < 5; ++operation)
        {
            for (const auto point : { Point::zone_prepared, Point::activity_prepared, Point::before_entity_commit })
            {
                if (operation == 2 && point != Point::before_entity_commit) continue;
                if (operation >= 3 && point == Point::before_entity_commit) continue;
                for (const bool existing : { false, true })
                {
                    w::World world;
                    spawn(world);
                    if (existing) spawn(world, 3, 2, false);
                    schedule(world, 10, s::RegionNotice{ 1, 7 });
                    const auto before = snapshot(world);
                    failure_point = point;
                    Access::hook(world, inject);
                    bool failed = false;
                    try
                    {
                        if (operation == 0) (void)world.move_entity_to_zone(1, 2, epoch);
                        else if (operation == 1) (void)world.spawn_entity(2, blueprint(), 2, epoch);
                        else if (operation == 2) (void)world.erase_entity(1, epoch);
                        else if (operation == 3) (void)world.wake_zone(2);
                        else (void)world.sleep_zone(2);
                    }
                    catch (const std::bad_alloc&) { failed = true; }
                    Access::hook(world, nullptr);
                    t::require(failed, "injected stage reached");
                    t::require_equal(before, snapshot(world), "preparation rollback");
                    assert_faulted(world);
                }
            }
        }
    }
    auto allocation_failures(std::string&) -> void
    {
        for (int operation = 0; operation < 4; ++operation)
        {
            bool completed = false;
            std::size_t failures = 0;
            for (std::size_t budget = 0; budget < 64 && !completed; ++budget)
            {
                w::World world;
                spawn(world);
                const auto before = snapshot(world);
                bool failed = false;
                bool accepted = false;
                try
                {
                    t::AllocationFailure allocation{ budget };
                    if (operation == 0) accepted = world.move_entity_to_zone(1, 2, epoch);
                    else if (operation == 1) accepted = world.spawn_entity(2, blueprint(), 2, epoch);
                    else if (operation == 2) accepted = world.wake_zone(2);
                    else accepted = world.sleep_zone(2);
                }
                catch (const std::bad_alloc&) { failed = true; }
                if (failed)
                {
                    ++failures;
                    t::require_equal(before, snapshot(world), "allocation failure rollback");
                    assert_faulted(world);
                }
                else
                {
                    completed = true;
                    t::require(accepted && world.has_consistent_spatial_state(), "operation succeeds after all allocation sites");
                }
            }
            t::require(completed && failures > 0, "all operation allocation sites exercised");
        }
        w::World world;
        spawn(world);
        bool erased = false;
        { t::AllocationFailure no_allocations{ 0 }; erased = world.erase_entity(1, epoch); }
        t::require(erased && world.has_consistent_spatial_state(), "erase commit needs no allocation");
    }
    auto facts_and_ownership(std::string&) -> void
    {
        w::World world;
        spawn(world);
        schedule(world, 1, s::CompleteMigration{ 1, 2 });
        schedule(world, 2, s::CompleteMigration{ 999, 3 });
        const auto stats = step(world);
        t::require_equal(std::size_t{ 1 }, stats.domain_events.size(), "only committed migration emits");
        t::require(stats.scheduled_results[0].accepted() && !stats.scheduled_results[1].accepted(), "structured results");
        t::require(world.event_queue_empty() && world.has_consistent_spatial_state(), "both outcomes acknowledged");
        t::require(!world.find_zone(3), "rejected action did not prepare destination");

        for (const bool scheduled : { false, true })
        {
            for (const auto point : { Point::before_entity_commit, Point::before_publish })
            {
                w::World failing;
                spawn(failing);
                if (scheduled)
                {
                    schedule(failing, 1, s::CompleteMigration{ 1, 2 });
                    schedule(failing, 2, s::RegionNotice{ 2, 10 });
                }
                const auto before = snapshot(failing);
                Access::hook(failing, inject);
                failure_point = point;
                bool returned = false;
                bool threw = false;
                try
                {
                    const auto ignored = scheduled ? step(failing) : command(failing, c::MoveToZone{ 2 });
                    (void)ignored;
                    returned = true;
                }
                catch (const std::bad_alloc&) { threw = true; }
                Access::hook(failing, nullptr);
                t::require(threw && !returned, "failed tick publishes no facts or results");
                if (point == Point::before_entity_commit) t::require_equal(before, snapshot(failing), "precommit exception restores state");
                else t::require_equal(std::uint64_t{ 2 }, failing.find_entity(1)->placement.zone_id(), "postcommit exception does not undo committed state");
                if (scheduled) t::require_equal(std::size_t{ 2 }, failing.pending_event_count(), "scheduler retains interrupted and unexecuted actions");
                assert_faulted(failing);
            }
        }
    }
    auto inventory_allocation_failures(std::string&) -> void
    {
        core::item::Catalog items;
        core::item::Definition definition{};
        definition.identity.item_template_id = 1;
        t::require(items.insert(definition), "item fixture");
        bool completed = false;
        std::size_t failures = 0;
        for (std::size_t budget = 0; budget < 64 && !completed; ++budget)
        {
            w::World world;
            spawn(world);
            core::status::Instance shield{};
            shield.kind = core::status::Kind::shield;
            core::status::ShieldLayer layer{};
            layer.absorb_percent_of_max_hp = 10;
            shield.shield_layers.push_back(layer);
            t::require(world.apply_status(1, shield), "shield fixture");
            const auto before = snapshot(world);
            core::item::Instance item{};
            item.item_id = 10;
            item.item_template_id = 1;
            bool failed = false;
            core::inventory::AddResult result;
            try
            {
                t::AllocationFailure allocation{ budget };
                result = world.add_inventory_item(1, items, item);
            }
            catch (const std::bad_alloc&) { failed = true; }
            if (failed)
            {
                ++failures;
                t::require_equal(before, snapshot(world), "inventory and derived load unchanged on exception");
                assert_faulted(world);
            }
            else
            {
                completed = true;
                t::require(result.quantity_added == 1 && !world.find_entity(1)->load.inventory_dirty, "inventory commit includes derived load");
                const auto committed = snapshot(world);
                t::require(world.add_inventory_item(1, items, item).quantity_added == 0, "duplicate inventory rejection");
                t::require_equal(committed, snapshot(world), "inventory rejection unchanged");
            }
        }
        t::require(completed && failures > 0, "inventory allocation boundaries exercised");
    }
    auto tick_allocation_failures(std::string&) -> void
    {
        bool completed = false;
        std::size_t before_commit = 0, after_commit = 0;
        for (std::size_t budget = 0; budget < 64 && !completed; ++budget)
        {
            w::World world;
            spawn(world);
            schedule(world, 1, s::CompleteMigration{ 1, 2 });
            c::Inbox inbox;
            t::require(inbox.try_push({ 0, { 1 }, 1, c::MoveToZone{ 3 } }, 0).accepted(), "tick fixture");
            const auto captured = inbox.capture_for_tick(0);
            const auto before = snapshot(world);
            std::optional<w::TickStats> published;
            bool failed = false;
            try
            {
                t::AllocationFailure allocation{ budget };
                published.emplace(w::step(world, {}, { 0, epoch }, captured.batch));
            }
            catch (const std::bad_alloc&) { failed = true; }
            if (failed)
            {
                t::require(!published, "no partial output escapes step");
                if (world.find_entity(1)->placement.zone_id() == 1)
                {
                    ++before_commit;
                    t::require_equal(before, snapshot(world), "failed preparation retains action and spatial state");
                }
                else ++after_commit;
                assert_faulted(world);
            }
            else
            {
                completed = true;
                t::require(published && published->domain_events.size() == 2 && world.event_queue_empty(), "complete output after both commits");
            }
        }
        t::require(completed && before_commit > 0 && after_commit > 0, "precommit rollback and postcommit fail-fast exercised separately");
    }
    auto repeat_stress(std::string&) -> void
    {
        const auto run = []()
        {
            w::World world;
            std::string trace;
            for (std::uint64_t i = 1; i <= 128; ++i)
            {
                spawn(world, i, 1, i % 2 == 1);
                t::require(world.has_consistent_spatial_state(), "stress spawn");
                t::require(world.move_entity_to_zone(i, 2, epoch), "stress move");
                t::require(world.has_consistent_spatial_state(), "stress move invariant");
                t::require(world.wake_zone(3), "stress wake");
                t::require(world.has_consistent_spatial_state(), "stress wake invariant");
                t::require(world.sleep_zone(3), "stress sleep");
                t::require(world.has_consistent_spatial_state(), "stress sleep invariant");
                schedule(world, i, s::CompleteMigration{ i, 3 });
                c::Inbox inbox;
                t::require(inbox.try_push({ i, { i }, i, c::MoveToZone{ 4 } }, i).accepted(), "stress ingress");
                const auto stats = w::step(world, {}, { i, epoch + core::time::Milliseconds{ static_cast<std::int64_t>(i) } }, inbox.capture_for_tick(i).batch);
                t::require(stats.domain_events.size() == 2 && stats.scheduled_results[0].accepted() && stats.command_results[0].accepted(), "stress facts and results");
                t::require(world.has_consistent_spatial_state(), "stress tick invariant");
                trace += output(stats);
                t::require(world.erase_entity(i, epoch), "stress erase");
                t::require(world.has_consistent_spatial_state(), "stress erase invariant");
            }
            return trace + snapshot(world);
        };
        t::require_equal(run(), run(), "same state, facts and results on deterministic repeat");
    }
}

int main()
{
    std::vector<t::TestResult> results;
    results.push_back(t::run_test("atomic.move_success", move_success));
    results.push_back(t::run_test("atomic.move_rejected", move_rejected));
    results.push_back(t::run_test("atomic.spawn_erase", spawn_erase));
    results.push_back(t::run_test("atomic.preparation_injection", injected_preparation));
    results.push_back(t::run_test("atomic.allocation_failures", allocation_failures));
    results.push_back(t::run_test("atomic.facts_and_scheduler_ownership", facts_and_ownership));
    results.push_back(t::run_test("atomic.inventory_allocation_failures", inventory_allocation_failures));
    results.push_back(t::run_test("atomic.tick_allocation_failures", tick_allocation_failures));
    results.push_back(t::run_test("atomic.invariant_stress_and_repeat", repeat_stress));
    return t::report_results("Atomic authoritative mutations", results, std::cout);
}
