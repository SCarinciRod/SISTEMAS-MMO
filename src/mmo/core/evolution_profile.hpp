#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

#include "id.hpp"
#include "trigger.hpp"

namespace mmo
{
    namespace core
    {
        namespace evolution_profile
        {
            struct Step
            {
                std::string_view name{};
                id::SpeciesId target_species_id{ id::invalid_species_id };
                std::vector<trigger::Requirement> requirements;
            };

            struct Profile
            {
                id::SpeciesId species_id{ id::invalid_species_id };
                std::string_view display_name{};
                std::vector<Step> steps;
            };

            class Table
            {
            public:
                auto insert(const Profile& profile) -> bool
                {
                    auto insert_result = profiles_.emplace(profile.species_id, profile);
                    (void)insert_result.first;
                    const bool inserted = insert_result.second;
                    return inserted;
                }

                auto erase(id::SpeciesId species_id) -> bool
                {
                    return profiles_.erase(species_id) > 0;
                }

                [[nodiscard]] auto find(id::SpeciesId species_id) -> Profile*
                {
                    auto profile_it = profiles_.find(species_id);
                    if (profile_it == profiles_.end())
                    {
                        return nullptr;
                    }

                    return &profile_it->second;
                }

                [[nodiscard]] auto find(id::SpeciesId species_id) const -> const Profile*
                {
                    auto profile_it = profiles_.find(species_id);
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
                std::unordered_map<id::SpeciesId, Profile> profiles_;
            };
        }
    }
}
