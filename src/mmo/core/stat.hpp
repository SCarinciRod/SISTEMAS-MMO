#pragma once

#include <algorithm>
#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace stat
        {
            struct Primary
            {
                std::int32_t vitality{ 0 };
                std::int32_t strength{ 0 };
                std::int32_t agility{ 0 };
                std::int32_t intellect{ 0 };
                std::int32_t faith{ 0 };
                std::int32_t dexterity{ 0 };
                std::int32_t luck{ 0 };
            };

            struct Derived
            {
                std::int32_t max_hp{ 0 };
                std::int32_t max_mana{ 0 };
                std::int32_t attack{ 0 };
                std::int32_t attack_speed{ 0 };
                std::int32_t magic_attack{ 0 };
                std::int32_t cast_speed{ 0 };
                std::int32_t defense{ 0 };
                std::int32_t magic_defense{ 0 };
                std::int32_t crit_chance{ 0 };
                std::int32_t crit_resist{ 0 };
                std::int32_t magic_crit{ 0 };
                std::int32_t magic_crit_res{ 0 };
                std::int32_t move_speed{ 0 };
            };

            [[nodiscard]] constexpr auto scale(std::int32_t value, std::int32_t numerator, std::int32_t denominator) -> std::int32_t
            {
                return (value <= 0 || denominator == 0) ? 0 : (value * numerator) / denominator;
            }

            [[nodiscard]] inline auto derive(const Primary& base) -> Derived
            {
                Derived derived{};
                derived.max_hp = 100 + scale(base.vitality, 30, 1) + scale(base.strength, 7, 1) + scale(base.faith, 4, 1);
                derived.max_mana = 50 + scale(base.intellect, 28, 1) + scale(base.faith, 16, 1);

                derived.attack = 10 + scale(base.strength, 11, 2) + scale(base.dexterity, 1, 2) + scale(base.luck, 1, 4);
                derived.attack_speed = 100 + scale(base.agility, 13, 5) + scale(base.luck, 1, 5);
                derived.magic_attack = 8 + scale(base.intellect, 11, 2) + scale(base.faith, 4, 1) + scale(base.luck, 1, 4);
                derived.cast_speed = 100 + scale(base.agility, 9, 10) + scale(base.intellect, 4, 5) + scale(base.faith, 7, 10) + scale(base.luck, 1, 5);

                derived.defense = 12 + scale(base.vitality, 7, 1) + scale(base.strength, 5, 4) + scale(base.luck, 1, 4);
                derived.magic_defense = 12 + scale(base.faith, 7, 1) + scale(base.intellect, 5, 4) + scale(base.luck, 1, 4);

                derived.crit_chance = 5 + scale(base.dexterity, 3, 2) + scale(base.luck, 2, 1) + scale(base.agility, 1, 4);
                derived.crit_resist = scale(base.vitality, 1, 1) + scale(base.luck, 4, 5) + scale(base.faith, 1, 4);
                derived.magic_crit = 5 + scale(base.intellect, 3, 2) + scale(base.faith, 1, 1) + scale(base.luck, 3, 2) + scale(base.dexterity, 1, 5);
                derived.magic_crit_res = scale(base.faith, 1, 1) + scale(base.intellect, 1, 2) + scale(base.luck, 1, 1);
                derived.move_speed = 100 + scale(base.agility, 6, 5) + scale(base.strength, 1, 4) + scale(base.luck, 1, 5);
                return derived;
            }

            [[nodiscard]] inline auto add(const Primary& lhs, const Primary& rhs) -> Primary
            {
                return Primary{
                    lhs.vitality + rhs.vitality,
                    lhs.strength + rhs.strength,
                    lhs.agility + rhs.agility,
                    lhs.intellect + rhs.intellect,
                    lhs.faith + rhs.faith,
                    lhs.dexterity + rhs.dexterity,
                    lhs.luck + rhs.luck
                };
            }

            [[nodiscard]] inline auto add(const Derived& lhs, const Derived& rhs) -> Derived
            {
                return Derived{
                    lhs.max_hp + rhs.max_hp,
                    lhs.max_mana + rhs.max_mana,
                    lhs.attack + rhs.attack,
                    lhs.attack_speed + rhs.attack_speed,
                    lhs.magic_attack + rhs.magic_attack,
                    lhs.cast_speed + rhs.cast_speed,
                    lhs.defense + rhs.defense,
                    lhs.magic_defense + rhs.magic_defense,
                    lhs.crit_chance + rhs.crit_chance,
                    lhs.crit_resist + rhs.crit_resist,
                    lhs.magic_crit + rhs.magic_crit,
                    lhs.magic_crit_res + rhs.magic_crit_res,
                    lhs.move_speed + rhs.move_speed
                };
            }

            inline auto clamp_non_negative(Primary& block) -> void
            {
                block.vitality = std::max(block.vitality, 0);
                block.strength = std::max(block.strength, 0);
                block.agility = std::max(block.agility, 0);
                block.intellect = std::max(block.intellect, 0);
                block.faith = std::max(block.faith, 0);
                block.dexterity = std::max(block.dexterity, 0);
                block.luck = std::max(block.luck, 0);
            }

            inline auto clamp_non_negative(Derived& block) -> void
            {
                block.max_hp = std::max(block.max_hp, 0);
                block.max_mana = std::max(block.max_mana, 0);
                block.attack = std::max(block.attack, 0);
                block.attack_speed = std::max(block.attack_speed, 0);
                block.magic_attack = std::max(block.magic_attack, 0);
                block.cast_speed = std::max(block.cast_speed, 0);
                block.defense = std::max(block.defense, 0);
                block.magic_defense = std::max(block.magic_defense, 0);
                block.crit_chance = std::max(block.crit_chance, 0);
                block.crit_resist = std::max(block.crit_resist, 0);
                block.magic_crit = std::max(block.magic_crit, 0);
                block.magic_crit_res = std::max(block.magic_crit_res, 0);
                block.move_speed = std::max(block.move_speed, 0);
            }
        }
    }
}
