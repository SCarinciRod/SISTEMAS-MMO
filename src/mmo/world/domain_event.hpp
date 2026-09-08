#pragma once

#include <variant>
#include "mmo/world/command.hpp"

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
    using Payload = std::variant<EntityMoved, HealthAdjusted, InventoryItemAdded>;

    // Per-tick facts in command order, separate from scheduling and rejections.
    struct Event
    {
        core::time::TickCount tick;
        core::time::TimePoint occurred_at;
        command::CommandId command_id;
        Payload payload;
    };
}
