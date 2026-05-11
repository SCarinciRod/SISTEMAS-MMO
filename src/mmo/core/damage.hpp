#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "id.hpp"

namespace mmo
{
    namespace core
    {
        namespace damage
        {
            // Damage contract: kinds, sources, packets, and traces for combat logging.
            enum class Kind : std::uint8_t
            {
                slash = 0,
                pierce,
                sever,
                impact,
                magic,
                elemental
            };

            enum class SourceKind : std::uint8_t
            {
                unknown = 0,
                entity,
                item,
                skill,
                status,
                environment,
                trap,
                system
            };

            struct Source
            {
                SourceKind kind{ SourceKind::unknown };

                std::optional<id::EntityId> entity_id{};
                std::optional<id::ItemId> item_id{};

                std::string_view ability_key{};
                std::string_view action_key{};
                std::string_view status_key{};

                std::uint32_t sequence{ 0 };
            };

            struct Packet
            {
                Kind kind{ Kind::impact };

                std::int32_t amount{ 0 };
                std::int32_t penetration_percent{ 0 };
                bool can_ricochet{ false };
                bool can_embed{ false };

                Source source{};
            };

            struct Trace
            {
                Source source{};
                Kind kind{ Kind::impact };

                std::int32_t incoming_damage{ 0 };
                std::int32_t mitigated_damage{ 0 };
                std::int32_t shield_absorbed{ 0 };
                std::int32_t health_damage{ 0 };

                bool critical{ false };
                bool shield_broken{ false };
            };

            struct SourceContribution
            {
                Source source{};

                std::uint64_t instances{ 0 };
                std::int64_t incoming_damage{ 0 };
                std::int64_t mitigated_damage{ 0 };
                std::int64_t shield_absorbed{ 0 };
                std::int64_t health_damage{ 0 };
                std::uint64_t critical_instances{ 0 };
                std::uint64_t shield_breaks{ 0 };
            };

            // Classification helpers used by combat/material calculations.
            [[nodiscard]] constexpr auto is_physical(Kind kind) -> bool
            {
                switch (kind)
                {
                    case Kind::magic:
                    case Kind::elemental:
                        return false;
                    default:
                        return true;
                }
            }

            [[nodiscard]] constexpr auto is_magical(Kind kind) -> bool
            {
                return !is_physical(kind);
            }

            [[nodiscard]] inline auto same_source_identity(
                const Source& lhs,
                const Source& rhs) -> bool
            {
                return lhs.kind == rhs.kind &&
                       lhs.entity_id == rhs.entity_id &&
                       lhs.item_id == rhs.item_id &&
                       lhs.ability_key == rhs.ability_key &&
                       lhs.action_key == rhs.action_key &&
                       lhs.status_key == rhs.status_key;
            }

            [[nodiscard]] inline auto source_kind_name(SourceKind kind) -> std::string_view
            {
                switch (kind)
                {
                    case SourceKind::unknown:
                        return "unknown";
                    case SourceKind::entity:
                        return "entity";
                    case SourceKind::item:
                        return "item";
                    case SourceKind::skill:
                        return "skill";
                    case SourceKind::status:
                        return "status";
                    case SourceKind::environment:
                        return "environment";
                    case SourceKind::trap:
                        return "trap";
                    case SourceKind::system:
                        return "system";
                    default:
                        return "unknown";
                }
            }

            [[nodiscard]] inline auto kind_name(Kind kind) -> std::string_view
            {
                switch (kind)
                {
                    case Kind::slash:
                        return "slash";
                    case Kind::pierce:
                        return "pierce";
                    case Kind::sever:
                        return "sever";
                    case Kind::impact:
                        return "impact";
                    case Kind::magic:
                        return "magic";
                    case Kind::elemental:
                        return "elemental";
                    default:
                        return "unknown";
                }
            }

            [[nodiscard]] inline auto make_entity_source(
                id::EntityId entity_id,
                std::string_view action_key = {},
                std::string_view ability_key = {},
                std::uint32_t sequence = 0) -> Source
            {
                Source source{};
                source.kind = SourceKind::entity;
                source.entity_id = entity_id;
                source.action_key = action_key;
                source.ability_key = ability_key;
                source.sequence = sequence;
                return source;
            }

            [[nodiscard]] inline auto make_item_source(
                id::EntityId entity_id,
                id::ItemId item_id,
                std::string_view action_key = {},
                std::string_view ability_key = {},
                std::uint32_t sequence = 0) -> Source
            {
                Source source{};
                source.kind = SourceKind::item;
                source.entity_id = entity_id;
                source.item_id = item_id;
                source.action_key = action_key;
                source.ability_key = ability_key;
                source.sequence = sequence;
                return source;
            }

            [[nodiscard]] inline auto make_skill_source(
                id::EntityId entity_id,
                std::string_view ability_key,
                std::uint32_t sequence = 0) -> Source
            {
                Source source{};
                source.kind = SourceKind::skill;
                source.entity_id = entity_id;
                source.ability_key = ability_key;
                source.sequence = sequence;
                return source;
            }

            [[nodiscard]] inline auto make_status_source(
                id::EntityId entity_id,
                std::string_view status_key,
                std::uint32_t sequence = 0) -> Source
            {
                Source source{};
                source.kind = SourceKind::status;
                source.entity_id = entity_id;
                source.status_key = status_key;
                source.sequence = sequence;
                return source;
            }

            [[nodiscard]] inline auto make_environment_source(
                std::string_view action_key,
                std::uint32_t sequence = 0) -> Source
            {
                Source source{};
                source.kind = SourceKind::environment;
                source.action_key = action_key;
                source.sequence = sequence;
                return source;
            }

            // Aggregates per-hit traces and summarizes damage by source.
            class Ledger
            {
            public:
                auto record(const Trace& trace) -> void
                {
                    traces_.push_back(trace);

                    total_incoming_damage_ += trace.incoming_damage;
                    total_mitigated_damage_ += trace.mitigated_damage;
                    total_shield_absorbed_ += trace.shield_absorbed;
                    total_health_damage_ += trace.health_damage;

                    if (trace.critical)
                    {
                        ++critical_instances_;
                    }

                    if (trace.shield_broken)
                    {
                        ++shield_breaks_;
                    }
                }

                [[nodiscard]] auto traces() const noexcept -> const std::vector<Trace>&
                {
                    return traces_;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return traces_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return traces_.empty();
                }

                [[nodiscard]] auto total_incoming_damage() const noexcept -> std::int64_t
                {
                    return total_incoming_damage_;
                }

                [[nodiscard]] auto total_mitigated_damage() const noexcept -> std::int64_t
                {
                    return total_mitigated_damage_;
                }

                [[nodiscard]] auto total_shield_absorbed() const noexcept -> std::int64_t
                {
                    return total_shield_absorbed_;
                }

                [[nodiscard]] auto total_health_damage() const noexcept -> std::int64_t
                {
                    return total_health_damage_;
                }

                [[nodiscard]] auto critical_instances() const noexcept -> std::uint64_t
                {
                    return critical_instances_;
                }

                [[nodiscard]] auto shield_breaks() const noexcept -> std::uint64_t
                {
                    return shield_breaks_;
                }

                [[nodiscard]] auto source_count() const -> std::size_t
                {
                    std::vector<Source> sources{};

                    for (const auto& trace : traces_)
                    {
                        bool found = false;

                        for (const auto& source : sources)
                        {
                            if (same_source_identity(source, trace.source))
                            {
                                found = true;
                                break;
                            }
                        }

                        if (!found)
                        {
                            sources.push_back(trace.source);
                        }
                    }

                    return sources.size();
                }

                [[nodiscard]] auto contribution_for(const Source& source) const -> SourceContribution
                {
                    SourceContribution contribution{};
                    contribution.source = source;

                    for (const auto& trace : traces_)
                    {
                        if (!same_source_identity(trace.source, source))
                        {
                            continue;
                        }

                        ++contribution.instances;
                        contribution.incoming_damage += trace.incoming_damage;
                        contribution.mitigated_damage += trace.mitigated_damage;
                        contribution.shield_absorbed += trace.shield_absorbed;
                        contribution.health_damage += trace.health_damage;

                        if (trace.critical)
                        {
                            ++contribution.critical_instances;
                        }

                        if (trace.shield_broken)
                        {
                            ++contribution.shield_breaks;
                        }
                    }

                    return contribution;
                }

                [[nodiscard]] auto contributions() const -> std::vector<SourceContribution>
                {
                    std::vector<SourceContribution> grouped{};

                    for (const auto& trace : traces_)
                    {
                        bool found = false;

                        for (auto& contribution : grouped)
                        {
                            if (same_source_identity(contribution.source, trace.source))
                            {
                                ++contribution.instances;
                                contribution.incoming_damage += trace.incoming_damage;
                                contribution.mitigated_damage += trace.mitigated_damage;
                                contribution.shield_absorbed += trace.shield_absorbed;
                                contribution.health_damage += trace.health_damage;

                                if (trace.critical)
                                {
                                    ++contribution.critical_instances;
                                }

                                if (trace.shield_broken)
                                {
                                    ++contribution.shield_breaks;
                                }

                                found = true;
                                break;
                            }
                        }

                        if (!found)
                        {
                            SourceContribution contribution{};
                            contribution.source = trace.source;
                            contribution.instances = 1;
                            contribution.incoming_damage = trace.incoming_damage;
                            contribution.mitigated_damage = trace.mitigated_damage;
                            contribution.shield_absorbed = trace.shield_absorbed;
                            contribution.health_damage = trace.health_damage;
                            contribution.critical_instances = trace.critical ? 1u : 0u;
                            contribution.shield_breaks = trace.shield_broken ? 1u : 0u;

                            grouped.push_back(contribution);
                        }
                    }

                    return grouped;
                }

                auto clear() -> void
                {
                    traces_.clear();

                    total_incoming_damage_ = 0;
                    total_mitigated_damage_ = 0;
                    total_shield_absorbed_ = 0;
                    total_health_damage_ = 0;

                    critical_instances_ = 0;
                    shield_breaks_ = 0;
                }

            private:
                std::vector<Trace> traces_{};

                std::int64_t total_incoming_damage_{ 0 };
                std::int64_t total_mitigated_damage_{ 0 };
                std::int64_t total_shield_absorbed_{ 0 };
                std::int64_t total_health_damage_{ 0 };

                std::uint64_t critical_instances_{ 0 };
                std::uint64_t shield_breaks_{ 0 };
            };
        }
    }
}
