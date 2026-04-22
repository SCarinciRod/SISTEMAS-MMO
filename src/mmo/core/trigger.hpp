#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace trigger
        {
            enum class Kind : std::uint8_t
            {
                survive_time = 0,
                no_damage_time,
                kill_count,
                migration_completed,
                zone_path_completed,
                repeated_encounter,
                find_specific_creature,
                find_specific_item,
                zone_population_state,
                world_event_observed
            };

            enum class Scope : std::uint8_t
            {
                individual = 0,
                lineage,
                zone,
                world
            };

            enum class Tier : std::uint8_t
            {
                common = 0,
                uncommon,
                rare,
                elite,
                global
            };

            struct Requirement
            {
                Kind kind{ Kind::survive_time };
                Scope scope{ Scope::individual };
                Tier tier{ Tier::common };
                std::uint32_t value{ 0 };
                std::optional<time::Milliseconds> window{};
                std::optional<id::SpeciesId> target_species_id{};
                std::optional<id::EntityId> target_entity_id{};
                std::optional<id::ZoneId> target_zone_id{};
                std::string_view note{};
            };
        }
    }
}
