#pragma once

#include "compat.hpp"

#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace id
        {
            using EntityId = std::uint64_t;
            using EntityTemplateId = std::uint64_t;
            using ItemId = std::uint64_t;
            using ItemTemplateId = std::uint64_t;
            using ItemSetId = std::uint64_t;
            using SessionId = std::uint64_t;
            using AccountId = std::uint64_t;
            using PlayerId = std::uint64_t;
            using SkillId = std::uint64_t;
            using WorldId = std::uint64_t;
            using ZoneId = std::uint64_t;
            using SpeciesId = std::uint64_t;

            constexpr EntityId invalid_entity_id = 0;
            constexpr EntityTemplateId invalid_entity_template_id = 0;
            constexpr ItemId invalid_item_id = 0;
            constexpr ItemTemplateId invalid_item_template_id = 0;
            constexpr ItemSetId invalid_item_set_id = 0;
            constexpr SessionId invalid_session_id = 0;
            constexpr AccountId invalid_account_id = 0;
            constexpr PlayerId invalid_player_id = 0;
            constexpr SkillId invalid_skill_id = 0;
            constexpr WorldId invalid_world_id = 0;
            constexpr ZoneId invalid_zone_id = 0;
            constexpr SpeciesId invalid_species_id = 0;
        }
    }
}

