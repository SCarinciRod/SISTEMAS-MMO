#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "damage.hpp"

namespace mmo
{
    namespace core
    {
        namespace material
        {
            enum class Kind : std::uint8_t
            {
                wood = 0,
                stone,
                metal,
                flesh,
                bone,
                crystal,
                cloth,
                leather,
                glass,
                earth,
                ice
            };

            struct Resistance
            {
                std::int32_t slash_resistance_percent{ 0 };
                std::int32_t pierce_resistance_percent{ 0 };
                std::int32_t sever_resistance_percent{ 0 };
                std::int32_t impact_resistance_percent{ 0 };
                std::int32_t magic_resistance_percent{ 0 };
                std::int32_t elemental_resistance_percent{ 0 };
            };

            struct Definition
            {
                Kind kind{ Kind::wood };
                std::string_view name{};
                std::uint32_t max_durability{ 0 };
                bool breakable{ true };
                bool ricochet_enabled{ false };
                bool entangle_enabled{ false };
                Resistance resistance{};
            };

            struct State
            {
                Kind kind{ Kind::wood };
                std::uint32_t durability{ 0 };
                std::uint32_t max_durability{ 0 };
                bool broken{ false };
            };

            [[nodiscard]] inline auto make_state(const Definition& definition) -> State
            {
                return State{
                    definition.kind,
                    definition.max_durability,
                    definition.max_durability,
                    false
                };
            }

            [[nodiscard]] constexpr auto resistance_for(const Resistance& resistance, damage::Kind kind) -> std::int32_t
            {
                switch (kind)
                {
                    case damage::Kind::slash:
                        return resistance.slash_resistance_percent;
                    case damage::Kind::pierce:
                        return resistance.pierce_resistance_percent;
                    case damage::Kind::sever:
                        return resistance.sever_resistance_percent;
                    case damage::Kind::impact:
                        return resistance.impact_resistance_percent;
                    case damage::Kind::magic:
                        return resistance.magic_resistance_percent;
                    case damage::Kind::elemental:
                    default:
                        return resistance.elemental_resistance_percent;
                }
            }

            [[nodiscard]] inline auto resolve_damage(const Definition& definition, const damage::Packet& packet) -> std::int32_t
            {
                if (packet.amount <= 0)
                {
                    return 0;
                }

                const auto resistance_percent = resistance_for(definition.resistance, packet.kind);
                const auto effective_percent = std::max<std::int32_t>(0, 100 - resistance_percent + packet.penetration_percent);
                const auto resolved_damage = (static_cast<std::int64_t>(packet.amount) * effective_percent) / 100;
                return static_cast<std::int32_t>(std::max<std::int64_t>(resolved_damage, 0));
            }

            class Catalog
            {
            public:
                auto insert(const Definition& definition) -> bool
                {
                    auto insert_result = definitions_.emplace(definition.kind, definition);
                    (void)insert_result.first;
                    const bool inserted = insert_result.second;
                    return inserted;
                }

                auto erase(Kind kind) -> bool
                {
                    return definitions_.erase(kind) > 0;
                }

                [[nodiscard]] auto find(Kind kind) -> Definition*
                {
                    auto definition_it = definitions_.find(kind);
                    if (definition_it == definitions_.end())
                    {
                        return nullptr;
                    }

                    return &definition_it->second;
                }

                [[nodiscard]] auto find(Kind kind) const -> const Definition*
                {
                    auto definition_it = definitions_.find(kind);
                    if (definition_it == definitions_.end())
                    {
                        return nullptr;
                    }

                    return &definition_it->second;
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
                std::unordered_map<Kind, Definition> definitions_;
            };
        }
    }
}