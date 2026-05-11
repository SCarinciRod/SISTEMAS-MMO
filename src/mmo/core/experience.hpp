#pragma once

#include <algorithm>
#include <cstdint>

namespace mmo
{
    namespace core
    {
        namespace experience
        {
            // Experience contract: tiered curve and reward configuration for level scaling.
            struct CurveConfig
            {
                std::uint32_t tier_size{ 10 };
                std::uint64_t base{ 100 };
                std::uint64_t base_jump{ 200 };
                std::uint64_t growth{ 20 };
                std::uint64_t growth_step{ 10 };
            };

            struct RewardConfig
            {
                std::uint32_t tier_size{ 10 };
                std::uint64_t base{ 15 };
                std::uint64_t base_jump{ 30 };
                std::uint64_t growth{ 4 };
                std::uint64_t growth_step{ 2 };
            };

            struct GainResult
            {
                std::uint32_t level{ 1 };
                std::uint64_t experience{ 0 };
                std::uint64_t experience_to_next{ 0 };
                std::uint32_t levels_gained{ 0 };
            };

            // Tiered curve calculation for required experience per level.
            [[nodiscard]] inline auto required_for_level(std::uint32_t level, const CurveConfig& config = {}) -> std::uint64_t
            {
                if (level == 0)
                {
                    level = 1;
                }

                const auto tier_size = std::max<std::uint32_t>(1, config.tier_size);
                const auto tier = (level - 1) / tier_size;
                const auto offset = (level - 1) % tier_size;

                const auto tier_u = static_cast<std::uint64_t>(tier);
                const auto tier_steps = static_cast<std::uint64_t>(tier_size > 0 ? tier_size - 1 : 0);
                const auto growth = static_cast<std::uint64_t>(config.growth) + (static_cast<std::uint64_t>(config.growth_step) * tier_u);

                const auto growth_sum = (tier_u * static_cast<std::uint64_t>(config.growth))
                    + (static_cast<std::uint64_t>(config.growth_step) * (tier_u * (tier_u > 0 ? tier_u - 1 : 0) / 2));
                const auto base = static_cast<std::uint64_t>(config.base)
                    + (tier_steps * growth_sum)
                    + (static_cast<std::uint64_t>(config.base_jump) * tier_u);

                return base + (growth * static_cast<std::uint64_t>(offset));
            }

            // Reward curve mirrors the tiered progression for drops or quests.
            [[nodiscard]] inline auto reward_for_level(std::uint32_t level, const RewardConfig& config = {}) -> std::uint64_t
            {
                if (level == 0)
                {
                    level = 1;
                }

                const auto tier_size = std::max<std::uint32_t>(1, config.tier_size);
                const auto tier = (level - 1) / tier_size;
                const auto offset = (level - 1) % tier_size;

                const auto tier_u = static_cast<std::uint64_t>(tier);
                const auto tier_steps = static_cast<std::uint64_t>(tier_size > 0 ? tier_size - 1 : 0);
                const auto growth = static_cast<std::uint64_t>(config.growth) + (static_cast<std::uint64_t>(config.growth_step) * tier_u);

                const auto growth_sum = (tier_u * static_cast<std::uint64_t>(config.growth))
                    + (static_cast<std::uint64_t>(config.growth_step) * (tier_u * (tier_u > 0 ? tier_u - 1 : 0) / 2));
                const auto base = static_cast<std::uint64_t>(config.base)
                    + (tier_steps * growth_sum)
                    + (static_cast<std::uint64_t>(config.base_jump) * tier_u);

                return base + (growth * static_cast<std::uint64_t>(offset));
            }

            // Apply gained experience across level boundaries using the curve above.
            [[nodiscard]] inline auto gain(
                std::uint32_t level,
                std::uint64_t experience,
                std::uint64_t experience_to_next,
                std::uint64_t gained,
                const CurveConfig& config = {}) -> GainResult
            {
                GainResult result{};
                result.level = (level == 0) ? 1 : level;
                result.experience = experience;
                result.experience_to_next = (experience_to_next == 0)
                    ? required_for_level(result.level, config)
                    : experience_to_next;

                while (gained > 0)
                {
                    if (result.experience + gained < result.experience_to_next)
                    {
                        result.experience += gained;
                        gained = 0;
                        break;
                    }

                    const auto remaining = result.experience_to_next - result.experience;
                    gained -= remaining;
                    result.experience = 0;
                    ++result.levels_gained;
                    ++result.level;
                    result.experience_to_next = required_for_level(result.level, config);
                }

                return result;
            }
        }
    }
}
