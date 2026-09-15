#include <iostream>
#include <type_traits>
#include "mmo/tests/support/test.hpp"
#include "mmo/world/world.hpp"

namespace
{
    namespace test = mmo::tests::support;
    namespace w = mmo::world;
    namespace s = w::scheduled;
    namespace d = w::domain;
    namespace c = w::command;
    using Tick = mmo::core::time::TickCount;
    const mmo::core::time::TimePoint epoch{};
    static_assert(!std::is_same_v<s::ScheduledAction, d::Event>);
    static_assert(!std::is_assignable_v<decltype((std::declval<d::Event&>().payload)), d::Payload>);

    auto spawn(w::World& world) -> void
    {
        mmo::core::entity::Blueprint blueprint{};
        blueprint.template_id = 1;
        blueprint.species_id = 1;
        blueprint.type = mmo::core::entity::Type::player;
        blueprint.base_primary = mmo::core::stat::Primary{ 12, 12, 8, 6, 5, 7, 4 };
        test::require(world.spawn_entity(1, blueprint, 1, epoch), "spawn");
    }
    auto step(w::World& world, Tick tick) -> w::TickStats
    {
        return w::step(world, {}, { tick, epoch + mmo::core::time::Milliseconds{ static_cast<std::int64_t>(tick) } }, c::Batch{ tick });
    }
    auto schedule(w::World& world, std::uint64_t id, Tick tick, s::Payload payload) -> void
    {
        test::require(world.try_schedule_action({ { id }, epoch + mmo::core::time::Milliseconds{ static_cast<std::int64_t>(tick) }, payload }).accepted(), "schedule");
    }
    auto equal_events(const std::vector<d::Event>& a, const std::vector<d::Event>& b) -> void
    {
        test::require_equal(a.size(), b.size(), "event count");
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            test::require(a[i].tick == b[i].tick && a[i].sequence == b[i].sequence && a[i].occurred_at == b[i].occurred_at, "identity and time");
            test::require_equal(a[i].cause.index(), b[i].cause.index(), "cause kind");
            const auto cause_id = [](const auto& cause) { return cause.id.value; };
            test::require_equal(std::visit(cause_id, a[i].cause), std::visit(cause_id, b[i].cause), "cause ID");
            test::require_equal(a[i].payload.index(), b[i].payload.index(), "fact kind");
            std::visit([&](const auto& lhs)
            {
                using T = std::decay_t<decltype(lhs)>;
                const auto& rhs = std::get<T>(b[i].payload);
                if constexpr (std::is_same_v<T, d::EntityMoved>)
                    test::require(lhs.entity_id == rhs.entity_id && lhs.source_zone_id == rhs.source_zone_id && lhs.destination_zone_id == rhs.destination_zone_id, "move payload");
                else if constexpr (std::is_same_v<T, d::HealthAdjusted>)
                    test::require(lhs.entity_id == rhs.entity_id && lhs.previous_health == rhs.previous_health && lhs.current_health == rhs.current_health, "health payload");
                else if constexpr (std::is_same_v<T, d::InventoryItemAdded>)
                    test::require(lhs.entity_id == rhs.entity_id && lhs.template_id == rhs.template_id && lhs.quantity == rhs.quantity, "inventory payload");
                else if constexpr (std::is_same_v<T, d::RegionNoticeEmitted>)
                    test::require(lhs.zone_id == rhs.zone_id && lhs.notice_id == rhs.notice_id, "notice payload");
                else test::require_equal(lhs.zone_id, rhs.zone_id, "zone payload");
            }, a[i].payload);
        }
    }
    auto deadline(std::string&) -> void
    {
        w::World world;
        spawn(world);
        schedule(world, 7, 10, s::CompleteMigration{ 1, 2 });
        test::require(step(world, 9).domain_events.empty(), "future is not a fact");
        test::require_equal(std::uint64_t{ 1 }, world.find_entity(1)->placement.zone_id(), "unchanged before due");
        const auto due = step(world, 10);
        test::require_equal(std::size_t{ 1 }, due.domain_events.size(), "one migration fact");
        const auto& fact = std::get<d::EntityMoved>(due.domain_events[0].payload);
        test::require(fact.source_zone_id == 1 && fact.destination_zone_id == 2, "both zones");
        test::require_equal(std::uint64_t{ 7 }, std::get<d::ScheduledActionCause>(due.domain_events[0].cause).id.value, "action cause");
        test::require(due.domain_events[0].occurred_at == epoch + mmo::core::time::Milliseconds{ 10 }, "logical time");
        test::require(step(world, 11).domain_events.empty(), "exactly once");
    }
    auto rejections_and_noops(std::string&) -> void
    {
        w::World world;
        spawn(world);
        test::require(!world.try_schedule_action({ { 1 }, epoch, s::WakeZone{ 0 } }).accepted(), "invalid payload blocked");
        test::require(!world.try_schedule_action({ { 0 }, epoch, s::WakeZone{ 1 } }).accepted(), "invalid id blocked");
        schedule(world, 1, 0, s::SleepZone{ 1 });
        test::require_equal(s::Rejection::duplicate_id, world.try_schedule_action({ { 1 }, epoch, s::WakeZone{ 2 } }).reason, "duplicate blocked");
        schedule(world, 2, 0, s::CompleteMigration{ 999, 2 });
        schedule(world, 3, 0, s::EvolutionDue{ 1 });
        schedule(world, 4, 0, s::CompleteMigration{ 1, 1 });
        const auto stats = step(world, 0);
        test::require(stats.domain_events.empty(), "no false facts");
        test::require_equal(std::uint64_t{ 3 }, stats.events_rejected, "three rejected");
        test::require_equal(s::Rejection::unsupported, stats.scheduled_results[2].reason, "evolution is unimplemented");
        test::require(stats.scheduled_results[3].accepted(), "same zone accepted as no-op");
        test::require(world.has_consistent_spatial_state(), "invariants");
    }
    auto wake_sleep(std::string&) -> void
    {
        w::World world;
        schedule(world, 1, 0, s::WakeZone{ 2 });
        schedule(world, 2, 0, s::WakeZone{ 2 });
        schedule(world, 3, 1, s::SleepZone{ 2 });
        schedule(world, 4, 1, s::SleepZone{ 2 });
        const auto wake = step(world, 0);
        test::require_equal(std::size_t{ 1 }, wake.domain_events.size(), "one wake transition");
        test::require(std::holds_alternative<d::ZoneWoken>(wake.domain_events[0].payload), "wake fact");
        const auto sleep = step(world, 1);
        test::require_equal(std::size_t{ 1 }, sleep.domain_events.size(), "one sleep transition");
        test::require(std::holds_alternative<d::ZoneSlept>(sleep.domain_events[0].payload), "sleep fact");
        test::require(!world.should_tick_full(2), "asleep");
    }
    auto deterministic_stream(std::string&) -> void
    {
        w::World first, second;
        spawn(first); spawn(second);
        mmo::core::item::Catalog items;
        mmo::core::item::Definition item{};
        item.identity.item_template_id = 1;
        test::require(items.insert(item), "item definition");
        for (auto* world : { &first, &second })
        {
            if (world == &first)
            {
                schedule(*world, 20, 5, s::CompleteMigration{ 1, 3 });
                schedule(*world, 10, 5, s::CompleteMigration{ 1, 2 });
            }
            else
            {
                schedule(*world, 10, 5, s::CompleteMigration{ 1, 2 });
                schedule(*world, 20, 5, s::CompleteMigration{ 1, 3 });
            }
            schedule(*world, 30, 6, s::RegionNotice{ 3, 42 });
        }
        c::Inbox a, b;
        const c::Envelope move{ 5, { 1 }, 1, c::MoveToZone{ 4 } };
        const c::Envelope health{ 5, { 2 }, 1, c::AdjustHealth{ -1 } };
        const c::Envelope grant{ 5, { 3 }, 1, c::AddInventoryItem{ 100, 1 } };
        for (const auto& command : { grant, health, move }) test::require(a.try_push(command, 5).accepted(), "input A");
        for (const auto& command : { move, grant, health }) test::require(b.try_push(command, 5).accepted(), "input B");
        const auto ca = a.capture_for_tick(5), cb = b.capture_for_tick(5);
        const w::TickContext context{ 5, epoch + mmo::core::time::Milliseconds{ 5 } };
        const auto sa = w::step(first, items, context, ca.batch);
        const auto sb = w::step(second, items, context, cb.batch);
        equal_events(sa.domain_events, sb.domain_events);
        test::require_equal(std::size_t{ 5 }, sa.domain_events.size(), "two scheduled plus three commands");
        for (std::size_t i = 0; i < sa.domain_events.size(); ++i)
            test::require_equal(static_cast<std::uint64_t>(i), sa.domain_events[i].id().sequence, "canonical fact id");
        test::require(std::holds_alternative<d::ScheduledActionCause>(sa.domain_events[1].cause), "scheduled first");
        test::require(std::holds_alternative<d::CommandCause>(sa.domain_events[2].cause), "command follows");
        test::require_equal(std::uint64_t{ 3 }, std::get<d::EntityMoved>(sa.domain_events[2].payload).source_zone_id, "command observes scheduled move");
        test::require_equal(std::uint64_t{ 4 }, first.find_entity(1)->placement.zone_id(), "final zone");
        test::require_equal(first.find_entity(1)->resources.health_current, second.find_entity(1)->resources.health_current, "same health");
        test::require_equal(first.find_entity(1)->inventory.items.size(), second.find_entity(1)->inventory.items.size(), "same inventory");
        test::require(first.active_zone_ids() == second.active_zone_ids(), "same active zones");
        test::require(first.has_consistent_spatial_state() && second.has_consistent_spatial_state(), "consistent worlds");
        equal_events(step(first, 6).domain_events, step(second, 6).domain_events);
        test::require(step(first, 7).domain_events.empty(), "no retained output");
    }
}
int main()
{
    std::vector<test::TestResult> results;
    results.push_back(test::run_test("events.deadline_exactly_once", deadline));
    results.push_back(test::run_test("events.rejections_noops", rejections_and_noops));
    results.push_back(test::run_test("events.wake_sleep_transitions", wake_sleep));
    results.push_back(test::run_test("events.deterministic_mixed_stream", deterministic_stream));
    return test::report_results("Domain event model", results, std::cout);
}
