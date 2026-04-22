#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "entity.hpp"
#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace evolution
        {
            struct Rule
            {
                id::SpeciesId species_id{ id::invalid_species_id };
                std::string name;
                std::optional<time::Milliseconds> no_damage_for{};
                std::optional<id::ZoneId> required_zone_id{};
                std::uint32_t minimum_survivors{ 0 };
            };

            struct Context
            {
                const entity::Record* record{ nullptr };
                std::uint32_t survivors_on_route{ 0 };
                time::TimePoint now{};
            };

            [[nodiscard]] inline auto due_at(const Rule& rule, const Context& context) -> std::optional<time::TimePoint>
            {
                if (!rule.no_damage_for.has_value())
                {
                    return std::nullopt;
                }

                if (context.record == nullptr)
                {
                    return std::nullopt;
                }

                if (!context.record->lifecycle.last_damage_at.has_value())
                {
                    return std::nullopt;
                }

                return context.record->lifecycle.last_damage_at.value() + rule.no_damage_for.value();
            }

            [[nodiscard]] inline auto ready(const Rule& rule, const Context& context) -> bool
            {
                if (context.record == nullptr)
                {
                    return false;
                }

                if (rule.species_id != id::invalid_species_id && context.record->identity.species_id != rule.species_id)
                {
                    return false;
                }

                if (!context.record->lifecycle.alive)
                {
                    return false;
                }

                if (rule.no_damage_for.has_value())
                {
                    if (!context.record->lifecycle.last_damage_at.has_value())
                    {
                        return false;
                    }

                    if (context.now < context.record->lifecycle.last_damage_at.value() + rule.no_damage_for.value())
                    {
                        return false;
                    }
                }

                if (rule.required_zone_id.has_value() && context.record->placement.zone_id != rule.required_zone_id.value())
                {
                    return false;
                }

                if (context.survivors_on_route < rule.minimum_survivors)
                {
                    return false;
                }

                return true;
            }
        }
    }
}
