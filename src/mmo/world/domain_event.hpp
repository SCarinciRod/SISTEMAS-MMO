#pragma once

#include <variant>
#include "mmo/world/command.hpp"
#include "mmo/world/scheduled_action.hpp"

namespace mmo::world::domain
{
    struct EntityMoved
    {
        core::id::EntityId entity_id;
        core::id::ZoneId source_zone_id;
        core::id::ZoneId destination_zone_id;
    };
    struct HealthAdjusted
    {
        core::id::EntityId entity_id;
        std::int32_t previous_health;
        std::int32_t current_health;
    };
    struct InventoryItemAdded
    {
        core::id::EntityId entity_id;
        core::id::ItemTemplateId template_id;
        std::uint32_t quantity;
    };
    struct ZoneWoken { core::id::ZoneId zone_id; };
    struct ZoneSlept { core::id::ZoneId zone_id; };
    struct RegionNoticeEmitted { core::id::ZoneId zone_id; std::uint32_t notice_id; };
    using Payload = std::variant<EntityMoved, HealthAdjusted, InventoryItemAdded,
        ZoneWoken, ZoneSlept, RegionNoticeEmitted>;
    struct CommandCause { command::CommandId id; };
    struct ScheduledActionCause { scheduled::ActionId id; };
    using Cause = std::variant<CommandCause, ScheduledActionCause>;
    struct EventId { core::time::TickCount tick; std::uint64_t sequence; };

    // Immutable facts in mutation order, with timeline-local identity and input cause.
    struct Event
    {
        const core::time::TickCount tick;
        const std::uint64_t sequence;
        const core::time::TimePoint occurred_at;
        const Cause cause;
        const Payload payload;
        [[nodiscard]] auto id() const noexcept -> EventId { return { tick, sequence }; }
    };
}
