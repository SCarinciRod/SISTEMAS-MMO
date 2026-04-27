#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "id.hpp"

namespace mmo
{
    namespace core
    {
        namespace species
        {
            enum class Family : std::uint8_t
            {
                humanoid = 0,
                beast,
                elemental,
                mechanical,
                spirit
            };

            enum class Origin : std::uint8_t
            {
                natural = 0,
                mutant,
                demonic,
                undead,
                corrupted,
                hybrid,
                arcane
            };

            constexpr std::array<Family, 5> all_families{
                Family::humanoid,
                Family::beast,
                Family::elemental,
                Family::mechanical,
                Family::spirit
            };

            constexpr std::array<Origin, 7> all_origins{
                Origin::natural,
                Origin::mutant,
                Origin::demonic,
                Origin::undead,
                Origin::corrupted,
                Origin::hybrid,
                Origin::arcane
            };

            [[nodiscard]] inline constexpr auto index_of(Family family) noexcept -> std::size_t
            {
                return static_cast<std::size_t>(family);
            }

            [[nodiscard]] inline constexpr auto index_of(Origin origin) noexcept -> std::size_t
            {
                return static_cast<std::size_t>(origin);
            }

            [[nodiscard]] inline constexpr auto family_name(Family family) noexcept -> std::string_view
            {
                switch (family)
                {
                case Family::humanoid:
                    return "Humanoid";
                case Family::beast:
                    return "Beast";
                case Family::elemental:
                    return "Elemental";
                case Family::mechanical:
                    return "Mechanical";
                case Family::spirit:
                    return "Spirit";
                }

                return "Unknown";
            }

            [[nodiscard]] inline constexpr auto origin_name(Origin origin) noexcept -> std::string_view
            {
                switch (origin)
                {
                case Origin::natural:
                    return "Natural";
                case Origin::mutant:
                    return "Mutant";
                case Origin::demonic:
                    return "Demonic";
                case Origin::undead:
                    return "Undead";
                case Origin::corrupted:
                    return "Corrupted";
                case Origin::hybrid:
                    return "Hybrid";
                case Origin::arcane:
                    return "Arcane";
                }

                return "Unknown";
            }

            struct Definition
            {
                id::SpeciesId species_id{ id::invalid_species_id };
                Family family{ Family::humanoid };
                std::vector<Origin> origins;
                std::string_view display_name{};
            };

            class Table
            {
            public:
                auto insert(const Definition& definition) -> bool
                {
                    auto insert_result = definitions_.emplace(definition.species_id, definition);
                    auto& it = insert_result.first;
                    const bool inserted = insert_result.second;
                    if (!inserted)
                    {
                        return false;
                    }

                    family_index_[index_of(definition.family)].push_back(definition.species_id);
                    for (const auto origin : definition.origins)
                    {
                        origin_index_[index_of(origin)].push_back(definition.species_id);
                    }

                    return true;
                }

                auto erase(id::SpeciesId species_id) -> bool
                {
                    auto definition_it = definitions_.find(species_id);
                    if (definition_it == definitions_.end())
                    {
                        return false;
                    }

                    unlink_from_family(definition_it->second.family, species_id);
                    for (const auto origin : definition_it->second.origins)
                    {
                        unlink_from_origin(origin, species_id);
                    }

                    definitions_.erase(definition_it);
                    return true;
                }

                [[nodiscard]] auto find(id::SpeciesId species_id) -> Definition*
                {
                    auto definition_it = definitions_.find(species_id);
                    if (definition_it == definitions_.end())
                    {
                        return nullptr;
                    }

                    return &definition_it->second;
                }

                [[nodiscard]] auto find(id::SpeciesId species_id) const -> const Definition*
                {
                    auto definition_it = definitions_.find(species_id);
                    if (definition_it == definitions_.end())
                    {
                        return nullptr;
                    }

                    return &definition_it->second;
                }

                [[nodiscard]] auto list_by_family(Family family) const -> std::vector<const Definition*>
                {
                    std::vector<const Definition*> definitions;
                    const auto& species_ids = family_index_[index_of(family)];
                    definitions.reserve(species_ids.size());

                    for (const auto species_id : species_ids)
                    {
                        auto definition_it = definitions_.find(species_id);
                        if (definition_it != definitions_.end())
                        {
                            definitions.push_back(&definition_it->second);
                        }
                    }

                    return definitions;
                }

                [[nodiscard]] auto list_by_origin(Origin origin) const -> std::vector<const Definition*>
                {
                    std::vector<const Definition*> definitions;
                    const auto& species_ids = origin_index_[index_of(origin)];
                    definitions.reserve(species_ids.size());

                    for (const auto species_id : species_ids)
                    {
                        auto definition_it = definitions_.find(species_id);
                        if (definition_it != definitions_.end())
                        {
                            definitions.push_back(&definition_it->second);
                        }
                    }

                    return definitions;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return definitions_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return definitions_.empty();
                }

            private:
                std::unordered_map<id::SpeciesId, Definition> definitions_;
                std::array<std::vector<id::SpeciesId>, all_families.size()> family_index_{};
                std::array<std::vector<id::SpeciesId>, all_origins.size()> origin_index_{};

                auto unlink_from_family(Family family, id::SpeciesId species_id) -> void
                {
                    auto& species_ids = family_index_[index_of(family)];
                    for (auto it = species_ids.begin(); it != species_ids.end(); ++it)
                    {
                        if (*it == species_id)
                        {
                            species_ids.erase(it);
                            break;
                        }
                    }
                }

                auto unlink_from_origin(Origin origin, id::SpeciesId species_id) -> void
                {
                    auto& species_ids = origin_index_[index_of(origin)];
                    for (auto it = species_ids.begin(); it != species_ids.end(); ++it)
                    {
                        if (*it == species_id)
                        {
                            species_ids.erase(it);
                            break;
                        }
                    }
                }
            };
        }
    }
}
