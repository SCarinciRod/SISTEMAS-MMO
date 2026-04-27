#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace action
        {
            enum class Kind : std::uint8_t
            {
                idle = 0,
                auto_attack,
                skill,
                cast,
                dodge,
                guard,
                sprint,
                item_use,
                interact
            };

            enum class Outcome : std::uint8_t
            {
                pending = 0,
                hit,
                whiff,
                blocked,
                interrupted,
                canceled
            };

            struct StatBias
            {
                std::int32_t attack_speed{ 0 };
                std::int32_t cast_speed{ 0 };
                std::int32_t move_speed{ 0 };
                std::int32_t crit_chance{ 0 };
                std::int32_t crit_resist{ 0 };
                std::int32_t magic_crit{ 0 };
                std::int32_t magic_crit_res{ 0 };
                std::int32_t defense{ 0 };
                std::int32_t magic_defense{ 0 };
            };

            struct State
            {
                Kind kind{ Kind::idle };
                Outcome outcome{ Outcome::pending };
                std::uint32_t chain_step{ 0 };
                bool moving{ false };
                bool rooted{ false };
                time::TimePoint started_at{};
                std::optional<time::TimePoint> resolved_at{};
                std::optional<id::EntityId> target_entity_id{};
            };

            struct Timing
            {
                time::Milliseconds animation_base{ 0 };
                time::Milliseconds recovery_hit_base{ 0 };
                time::Milliseconds recovery_block_base{ 0 };
                time::Milliseconds recovery_whiff_base{ 0 };
                time::Milliseconds recovery_interrupt_base{ 0 };
            };

            struct Profile
            {
                Kind kind{ Kind::idle };
                std::string_view name{};
                std::uint32_t weight{ 100 };
                Timing timing{};
                std::int32_t tempo_percent{ 100 };
                std::int32_t animation_floor_percent{ 35 };
                std::int32_t recovery_floor_percent{ 35 };
                std::int32_t chain_step_percent{ 0 };
                std::int32_t moving_penalty_percent{ 0 };
                std::int32_t stationary_bonus_percent{ 0 };
                StatBias animation_bias{};
                StatBias recovery_bias{};
            };

            struct Result
            {
                Kind kind{ Kind::idle };
                Outcome outcome{ Outcome::pending };
                std::uint32_t weight{ 100 };
                time::Milliseconds animation{};
                time::Milliseconds recovery{};
                std::int32_t animation_tempo_percent{ 100 };
                std::int32_t recovery_tempo_percent{ 100 };
                std::int32_t animation_stat_bonus_percent{ 0 };
                std::int32_t recovery_stat_bonus_percent{ 0 };
            };

            class Catalog
            {
            public:
                auto insert(const Profile& profile) -> bool
                {
                    auto insert_result = profiles_.emplace(profile.kind, profile);
                    (void)insert_result.first;
                    const bool inserted = insert_result.second;
                    return inserted;
                }

                auto erase(Kind kind) -> bool
                {
                    return profiles_.erase(kind) > 0;
                }

                [[nodiscard]] auto find(Kind kind) -> Profile*
                {
                    auto profile_it = profiles_.find(kind);
                    if (profile_it == profiles_.end())
                    {
                        return nullptr;
                    }

                    return &profile_it->second;
                }

                [[nodiscard]] auto find(Kind kind) const -> const Profile*
                {
                    auto profile_it = profiles_.find(kind);
                    if (profile_it == profiles_.end())
                    {
                        return nullptr;
                    }

                    return &profile_it->second;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return profiles_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return profiles_.empty();
                }

            private:
                std::unordered_map<Kind, Profile> profiles_;
            };

            [[nodiscard]] inline auto make_auto_attack_profile() -> Profile
            {
                Profile profile{};
                profile.kind = Kind::auto_attack;
                profile.name = "Auto Attack";
                profile.weight = 100;
                profile.timing.animation_base = time::Milliseconds{ 220 };
                profile.timing.recovery_hit_base = time::Milliseconds{ 140 };
                profile.timing.recovery_block_base = time::Milliseconds{ 180 };
                profile.timing.recovery_whiff_base = time::Milliseconds{ 240 };
                profile.timing.recovery_interrupt_base = time::Milliseconds{ 280 };
                profile.tempo_percent = 100;
                profile.animation_floor_percent = 30;
                profile.recovery_floor_percent = 20;
                profile.chain_step_percent = 6;
                profile.moving_penalty_percent = 10;
                profile.stationary_bonus_percent = 4;
                profile.animation_bias.attack_speed = 7;
                profile.animation_bias.move_speed = 2;
                profile.recovery_bias.attack_speed = 9;
                profile.recovery_bias.crit_chance = 1;
                return profile;
            }

            [[nodiscard]] inline auto make_dodge_profile() -> Profile
            {
                Profile profile{};
                profile.kind = Kind::dodge;
                profile.name = "Dodge";
                profile.weight = 80;
                profile.timing.animation_base = time::Milliseconds{ 140 };
                profile.timing.recovery_hit_base = time::Milliseconds{ 110 };
                profile.timing.recovery_block_base = time::Milliseconds{ 120 };
                profile.timing.recovery_whiff_base = time::Milliseconds{ 170 };
                profile.timing.recovery_interrupt_base = time::Milliseconds{ 220 };
                profile.tempo_percent = 100;
                profile.animation_floor_percent = 35;
                profile.recovery_floor_percent = 25;
                profile.chain_step_percent = 0;
                profile.moving_penalty_percent = 6;
                profile.stationary_bonus_percent = 8;
                profile.animation_bias.move_speed = 8;
                profile.recovery_bias.move_speed = 10;
                profile.recovery_bias.crit_resist = 1;
                return profile;
            }

            [[nodiscard]] inline auto make_skill_focused_catalog() -> Catalog
            {
                Catalog catalog{};
                catalog.insert(make_auto_attack_profile());
                catalog.insert(make_dodge_profile());
                return catalog;
            }
        }
    }
}