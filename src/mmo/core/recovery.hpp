#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "action.hpp"
#include "stat.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace recovery
        {
            [[nodiscard]] constexpr auto surplus(std::int32_t value, std::int32_t baseline) -> std::int32_t
            {
                return (value > baseline) ? (value - baseline) : 0;
            }

            [[nodiscard]] constexpr auto clamp_percent(std::int32_t value) -> std::int32_t
            {
                return (value < 1) ? 1 : value;
            }

            [[nodiscard]] inline auto bias_percent(const stat::Derived& derived, const action::StatBias& bias) -> std::int32_t
            {
                auto bonus_percent = std::int32_t{ 0 };
                bonus_percent += (surplus(derived.attack_speed, 100) * bias.attack_speed) / 10;
                bonus_percent += (surplus(derived.cast_speed, 100) * bias.cast_speed) / 10;
                bonus_percent += (surplus(derived.move_speed, 100) * bias.move_speed) / 10;
                bonus_percent += (surplus(derived.accuracy, 75) * bias.accuracy) / 10;
                bonus_percent += (surplus(derived.crit_chance, 5) * bias.crit_chance) / 10;
                bonus_percent += (surplus(derived.evasion, 5) * bias.evasion) / 10;
                bonus_percent += (surplus(derived.crit_resist, 0) * bias.crit_resist) / 10;
                bonus_percent += (surplus(derived.magic_crit, 5) * bias.magic_crit) / 10;
                bonus_percent += (surplus(derived.magic_crit_res, 0) * bias.magic_crit_res) / 10;
                bonus_percent += (surplus(derived.defense, 12) * bias.defense) / 10;
                bonus_percent += (surplus(derived.magic_defense, 12) * bias.magic_defense) / 10;
                return std::clamp(bonus_percent, 0, 85);
            }

            [[nodiscard]] inline auto adjust_duration(
                time::Milliseconds base,
                std::uint32_t weight,
                std::int32_t tempo_percent,
                std::int32_t floor_percent) -> time::Milliseconds
            {
                const auto safe_weight = std::max<std::uint32_t>(1, weight);
                const auto safe_tempo = clamp_percent(tempo_percent);
                const auto safe_floor = clamp_percent(floor_percent);

                const auto base_count = static_cast<std::int64_t>(base.count());
                const auto weighted_count = (base_count * safe_weight) / 100;
                const auto tempo_count = (weighted_count * 100) / safe_tempo;
                const auto floor_count = (weighted_count * safe_floor) / 100;
                const auto final_count = std::max<std::int64_t>(tempo_count, floor_count);

                return time::Milliseconds{ static_cast<time::Milliseconds::rep>(final_count) };
            }

            [[nodiscard]] inline auto resolve_recovery_base(const action::Timing& timing, action::Outcome outcome) -> time::Milliseconds
            {
                switch (outcome)
                {
                    case action::Outcome::hit:
                        return timing.recovery_hit_base;
                    case action::Outcome::blocked:
                        return (timing.recovery_block_base.count() > 0) ? timing.recovery_block_base : timing.recovery_whiff_base;
                    case action::Outcome::interrupted:
                        return (timing.recovery_interrupt_base.count() > 0) ? timing.recovery_interrupt_base : timing.recovery_whiff_base;
                    case action::Outcome::canceled:
                        return time::Milliseconds{ 0 };
                    case action::Outcome::pending:
                    case action::Outcome::whiff:
                    default:
                        return (timing.recovery_hit_base.count() > 0) ? timing.recovery_hit_base : timing.recovery_whiff_base;
                }
            }

            [[nodiscard]] inline auto calculate_animation(const stat::Derived& derived, const action::Profile& profile, const action::State& state) -> time::Milliseconds
            {
                auto tempo_percent = profile.tempo_percent
                    + (static_cast<std::int32_t>(state.chain_step) * profile.chain_step_percent)
                    - (state.moving ? profile.moving_penalty_percent : 0)
                    + ((state.rooted || !state.moving) ? profile.stationary_bonus_percent : 0)
                    + bias_percent(derived, profile.animation_bias);

                tempo_percent = std::max(tempo_percent, profile.animation_floor_percent);
                return adjust_duration(profile.timing.animation_base, profile.weight, tempo_percent, profile.animation_floor_percent);
            }

            [[nodiscard]] inline auto calculate_recovery(const stat::Derived& derived, const action::Profile& profile, const action::State& state) -> time::Milliseconds
            {
                const auto base_recovery = resolve_recovery_base(profile.timing, state.outcome);

                auto tempo_percent = profile.tempo_percent
                    + (static_cast<std::int32_t>(state.chain_step) * profile.chain_step_percent)
                    + bias_percent(derived, profile.recovery_bias);

                tempo_percent = std::max(tempo_percent, profile.recovery_floor_percent);
                return adjust_duration(base_recovery, profile.weight, tempo_percent, profile.recovery_floor_percent);
            }

            [[nodiscard]] inline auto calculate(const stat::Derived& derived, const action::Profile& profile, const action::State& state) -> action::Result
            {
                const auto animation_tempo_percent = profile.tempo_percent
                    + (static_cast<std::int32_t>(state.chain_step) * profile.chain_step_percent)
                    - (state.moving ? profile.moving_penalty_percent : 0)
                    + ((state.rooted || !state.moving) ? profile.stationary_bonus_percent : 0)
                    + bias_percent(derived, profile.animation_bias);

                const auto recovery_tempo_percent = profile.tempo_percent
                    + (static_cast<std::int32_t>(state.chain_step) * profile.chain_step_percent)
                    + bias_percent(derived, profile.recovery_bias);

                return action::Result{
                    profile.kind,
                    state.outcome,
                    profile.weight,
                    calculate_animation(derived, profile, state),
                    calculate_recovery(derived, profile, state),
                    std::max(animation_tempo_percent, profile.animation_floor_percent),
                    std::max(recovery_tempo_percent, profile.recovery_floor_percent),
                    bias_percent(derived, profile.animation_bias),
                    bias_percent(derived, profile.recovery_bias)
                };
            }

            [[nodiscard]] inline auto calculate(const stat::Derived& derived, const action::Catalog& catalog, const action::State& state) -> std::optional<action::Result>
            {
                auto profile = catalog.find(state.kind);
                if (profile == nullptr)
                {
                    return std::nullopt;
                }

                return calculate(derived, *profile, state);
            }
        }
    }
}
