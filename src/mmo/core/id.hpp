#pragma once

#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace id
        {
            using EntityId = std::uint64_t;
            using EntityTemplateId = std::uint64_t;
            using SessionId = std::uint64_t;
            using AccountId = std::uint64_t;
            using PlayerId = std::uint64_t;
            using WorldId = std::uint64_t;
            using ZoneId = std::uint64_t;
            using SpeciesId = std::uint64_t;

            inline constexpr EntityId invalid_entity_id = 0;
            inline constexpr EntityTemplateId invalid_entity_template_id = 0;
            inline constexpr SessionId invalid_session_id = 0;
            inline constexpr AccountId invalid_account_id = 0;
            inline constexpr PlayerId invalid_player_id = 0;
            inline constexpr WorldId invalid_world_id = 0;
            inline constexpr ZoneId invalid_zone_id = 0;
            inline constexpr SpeciesId invalid_species_id = 0;
        }
    }
}

