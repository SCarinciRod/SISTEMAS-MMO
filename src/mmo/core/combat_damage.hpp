#pragma once

#include <algorithm>
#include <cstdint>

#include "damage.hpp"

namespace mmo
{
    namespace core
    {
        namespace combat
        {
            struct DamageProfile
            {
                damage::Packet packet{};
                bool critical{ false };
                std::int32_t critical_multiplier_percent{ 150 };
            };

            struct DefenseProfile
            {
                std::int32_t physical_reduction_percent{ 0 };
                std::int32_t magical_reduction_percent{ 0 };
                std::int32_t flat_reduction{ 0 };
                std::int32_t shield_current{ 0 };
            };

            struct DamageResult
            {
                damage::Kind kind{ damage::Kind::impact };
                std::int32_t incoming_damage{ 0 };
                std::int32_t mitigated_damage{ 0 };
                std::int32_t shield_absorbed{ 0 };
                std::int32_t health_damage{ 0 };
                bool critical{ false };
                bool shield_broken{ false };
            };

            [[nodiscard]] inline auto resolve_damage(const DamageProfile& profile, const DefenseProfile& defense) -> DamageResult
            {
                auto result = DamageResult{};
                result.kind = profile.packet.kind;
                result.critical = profile.critical;

                auto incoming_damage = std::max<std::int32_t>(0, profile.packet.amount);
                if (result.critical)
                {
                    const auto critical_multiplier = std::max<std::int32_t>(100, profile.critical_multiplier_percent);
                    incoming_damage = static_cast<std::int32_t>((static_cast<std::int64_t>(incoming_damage) * critical_multiplier) / 100);
                }

                result.incoming_damage = incoming_damage;

                const auto reduction_percent = damage::is_physical(profile.packet.kind)
                    ? defense.physical_reduction_percent
                    : defense.magical_reduction_percent;
                const auto effective_percent = std::max<std::int32_t>(0, 100 - reduction_percent + profile.packet.penetration_percent);

                auto mitigated_damage = static_cast<std::int32_t>((static_cast<std::int64_t>(incoming_damage) * effective_percent) / 100);
                mitigated_damage = std::max<std::int32_t>(0, mitigated_damage - defense.flat_reduction);

                result.mitigated_damage = mitigated_damage;
                result.shield_absorbed = std::min(std::max<std::int32_t>(0, defense.shield_current), mitigated_damage);
                result.health_damage = mitigated_damage - result.shield_absorbed;
                result.shield_broken = result.shield_absorbed > 0 && defense.shield_current > 0 && result.shield_absorbed >= defense.shield_current;
                return result;
            }
        }
    }
}