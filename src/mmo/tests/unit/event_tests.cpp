#include "mmo/core/event.hpp"
#include "mmo/tests/support/test.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace test = mmo::tests::support;

    auto test_validation_rules(std::string& details) -> void
    {
        mmo::core::event::Event event{};
        event.type = mmo::core::event::Type::evolution_due;

        test::require_equal(
            mmo::core::event::ValidationIssue::missing_entity,
            mmo::core::event::validate(event),
            "evolution_due requires entity");

        event.entity_id = static_cast<mmo::core::id::EntityId>(1);
        test::require(mmo::core::event::is_valid(event), "evolution_due valid");
        details = "validation_ok";
    }

    auto test_scheduler_basic(std::string& details) -> void
    {
        mmo::core::event::Scheduler scheduler{};
        const auto epoch = mmo::core::time::TimePoint{};

        mmo::core::event::Event first{};
        first.type = mmo::core::event::Type::region_notice;
        first.due_at = epoch + mmo::core::time::Milliseconds{ 100 };
        first.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        first.counter = 1;

        mmo::core::event::Event second{};
        second.type = mmo::core::event::Type::region_notice;
        second.due_at = epoch + mmo::core::time::Milliseconds{ 50 };
        second.zone_id = static_cast<mmo::core::id::ZoneId>(1);
        second.counter = 2;

        test::require(scheduler.try_schedule(first), "schedule first");
        test::require(scheduler.try_schedule(second), "schedule second");

        const auto ready = scheduler.pop_ready(
            epoch + mmo::core::time::Milliseconds{ 75 });

        test::require_equal(static_cast<std::size_t>(1), ready.size(), "ready count");
        test::require_equal(static_cast<std::uint32_t>(2), ready.front().counter, "ordering");
        details = "ordering_ok";
    }

    auto test_scheduler_ordering(std::string& details) -> void
    {
        mmo::core::event::Scheduler scheduler{};
        const auto epoch = mmo::core::time::TimePoint{};

        for (const auto counter : { 3u, 1u, 2u })
        {
            mmo::core::event::Event event{};
            event.type = mmo::core::event::Type::region_notice;
            event.due_at = epoch + mmo::core::time::Milliseconds{
                static_cast<std::int64_t>(counter) * 100 };
            event.zone_id = static_cast<mmo::core::id::ZoneId>(1);
            event.counter = counter;
            scheduler.schedule(event);
        }

        const auto ready = scheduler.pop_ready(
            epoch + mmo::core::time::Milliseconds{ 250 });

        test::require_equal(static_cast<std::size_t>(2), ready.size(), "ready event count");
        test::require_equal(static_cast<std::uint32_t>(1), ready[0].counter, "first ready event");
        test::require_equal(static_cast<std::uint32_t>(2), ready[1].counter, "second ready event");
        test::require_equal(static_cast<std::size_t>(1), scheduler.size(), "remaining event count");

        const auto remaining = scheduler.pop_ready(
            epoch + mmo::core::time::Milliseconds{ 400 });

        test::require_equal(static_cast<std::size_t>(1), remaining.size(), "remaining ready count");
        test::require_equal(static_cast<std::uint32_t>(3), remaining.front().counter, "remaining event");
        test::require(scheduler.empty(), "scheduler empty after all events popped");
        details = "ordered_events=3";
    }
}

int main()
{
    std::vector<test::TestResult> results;
    results.push_back(test::run_test("unit.event.validation_rules", test_validation_rules));
    results.push_back(test::run_test("unit.event.scheduler_basic", test_scheduler_basic));
    results.push_back(test::run_test("unit.event.scheduler_ordering", test_scheduler_ordering));
    return test::report_results("Unit", results, std::cout);
}
