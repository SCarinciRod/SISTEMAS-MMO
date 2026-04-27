#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "id.hpp"
#include "stat.hpp"
#include "status.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace item
        {
            enum class Kind : std::uint8_t
            {
                material = 0,
                consumable,
                equipment,
                quest,
                collectible
            };

            enum class EquipmentClass : std::uint8_t
            {
                weapon = 0,
                armor,
                accessory,
                tool
            };

            enum class EquipmentSlot : std::uint8_t
            {
                none = 0,
                head,
                chest,
                hands,
                legs,
                feet,
                main_hand,
                off_hand,
                ring_1,
                ring_2,
                earring_1,
                earring_2,
                necklace,
                tool
            };

            struct Identity
            {
                id::ItemTemplateId item_template_id{ id::invalid_item_template_id };
                std::string_view name{};
                std::string_view description{};
            };

            struct StackRule
            {
                bool stackable{ false };
                std::uint32_t max_stack{ 1 };
            };

            struct BindingRule
            {
                bool droppable{ true };
                bool tradable{ true };
                bool discardable{ true };
                bool deletable{ true };
            };

            struct EffectProfile
            {
                stat::Primary primary_delta{};
                stat::Derived derived_delta{};
                std::vector<status::Kind> granted_statuses{};
            };

            struct MaterialDefinition
            {
                bool craft_only{ true };
                bool delivery_only{ true };
            };

            struct ConsumableDefinition
            {
                bool consumed_on_use{ true };
                EffectProfile effect{};
            };

            struct SetBonus
            {
                id::ItemSetId set_id{ id::invalid_item_set_id };
                std::uint32_t required_pieces{ 0 };
                EffectProfile effect{};
            };

            struct EquipmentDefinition
            {
                EquipmentClass equipment_class{ EquipmentClass::armor };
                EquipmentSlot slot{ EquipmentSlot::none };
                std::uint32_t max_durability{ 0 };
                bool two_handed{ false };
                EffectProfile effect{};
                std::vector<SetBonus> set_bonuses{};
            };

            struct QuestDefinition
            {
                std::string_view quest_key{};
                std::string_view quest_step_key{};
                bool locked_until_completion{ true };
                bool consumed_on_completion{ true };
            };

            struct CollectibleDefinition
            {
                std::string_view topic{};
                std::string_view lore{};
                std::string_view source{};
            };

            struct Definition
            {
                Identity identity{};
                Kind kind{ Kind::material };
                StackRule stack{};
                BindingRule binding{};
                std::optional<MaterialDefinition> material{};
                std::optional<ConsumableDefinition> consumable{};
                std::optional<EquipmentDefinition> equipment{};
                std::optional<QuestDefinition> quest{};
                std::optional<CollectibleDefinition> collectible{};
            };

            struct Instance
            {
                id::ItemId item_id{ id::invalid_item_id };
                id::ItemTemplateId item_template_id{ id::invalid_item_template_id };
                std::uint32_t quantity{ 1 };
                std::uint32_t durability{ 0 };
                bool equipped{ false };
                bool bound{ false };
                bool quest_locked{ false };
                std::optional<id::EntityId> owner_entity_id{};
                std::optional<time::TimePoint> acquired_at{};
            };

            class Catalog
            {
            public:
                auto insert(const Definition& definition) -> bool
                {
                    if (definition.identity.item_template_id == id::invalid_item_template_id)
                    {
                        return false;
                    }

                    auto insert_result = definitions_.emplace(definition.identity.item_template_id, definition);
                    (void)insert_result.first;
                    const bool inserted = insert_result.second;
                    return inserted;
                }

                auto erase(id::ItemTemplateId item_template_id) -> bool
                {
                    return definitions_.erase(item_template_id) > 0;
                }

                [[nodiscard]] auto find(id::ItemTemplateId item_template_id) -> Definition*
                {
                    auto definition_it = definitions_.find(item_template_id);
                    if (definition_it == definitions_.end())
                    {
                        return nullptr;
                    }

                    return &definition_it->second;
                }

                [[nodiscard]] auto find(id::ItemTemplateId item_template_id) const -> const Definition*
                {
                    auto definition_it = definitions_.find(item_template_id);
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
                std::unordered_map<id::ItemTemplateId, Definition> definitions_;
            };

            [[nodiscard]] constexpr auto is_stackable(Kind kind) -> bool
            {
                switch (kind)
                {
                    case Kind::material:
                    case Kind::consumable:
                        return true;
                    default:
                        return false;
                }
            }

            [[nodiscard]] constexpr auto is_equipment(Kind kind) -> bool
            {
                return kind == Kind::equipment;
            }

            [[nodiscard]] constexpr auto is_armor_slot(EquipmentSlot slot) -> bool
            {
                switch (slot)
                {
                    case EquipmentSlot::head:
                    case EquipmentSlot::chest:
                    case EquipmentSlot::hands:
                    case EquipmentSlot::legs:
                    case EquipmentSlot::feet:
                        return true;
                    default:
                        return false;
                }
            }

            [[nodiscard]] constexpr auto is_hand_slot(EquipmentSlot slot) -> bool
            {
                return slot == EquipmentSlot::main_hand || slot == EquipmentSlot::off_hand;
            }

            [[nodiscard]] constexpr auto is_accessory_slot(EquipmentSlot slot) -> bool
            {
                switch (slot)
                {
                    case EquipmentSlot::ring_1:
                    case EquipmentSlot::ring_2:
                    case EquipmentSlot::earring_1:
                    case EquipmentSlot::earring_2:
                    case EquipmentSlot::necklace:
                        return true;
                    default:
                        return false;
                }
            }

            [[nodiscard]] constexpr auto is_tool_slot(EquipmentSlot slot) -> bool
            {
                return slot == EquipmentSlot::tool;
            }

            [[nodiscard]] constexpr auto supports_equipment_slot(
                EquipmentClass equipment_class,
                EquipmentSlot slot,
                bool two_handed = false) -> bool
            {
                switch (equipment_class)
                {
                    case EquipmentClass::weapon:
                        return two_handed ? slot == EquipmentSlot::main_hand : is_hand_slot(slot);
                    case EquipmentClass::armor:
                        return is_armor_slot(slot);
                    case EquipmentClass::accessory:
                        return is_accessory_slot(slot);
                    case EquipmentClass::tool:
                        return is_tool_slot(slot);
                    default:
                        return false;
                }
            }

            [[nodiscard]] constexpr auto is_valid_set_bonus(const SetBonus& set_bonus) -> bool
            {
                return set_bonus.set_id != id::invalid_item_set_id && set_bonus.required_pieces > 0;
            }

            [[nodiscard]] inline auto find_set_bonus(const EquipmentDefinition& equipment, id::ItemSetId set_id) -> const SetBonus*
            {
                for (const auto& set_bonus : equipment.set_bonuses)
                {
                    if (set_bonus.set_id == set_id)
                    {
                        return &set_bonus;
                    }
                }

                return nullptr;
            }

            [[nodiscard]] inline auto has_set_bonus(const EquipmentDefinition& equipment, id::ItemSetId set_id) -> bool
            {
                return find_set_bonus(equipment, set_id) != nullptr;
            }

            [[nodiscard]] inline auto is_valid_equipment_definition(const EquipmentDefinition& equipment) -> bool
            {
                if (!supports_equipment_slot(equipment.equipment_class, equipment.slot, equipment.two_handed))
                {
                    return false;
                }

                for (const auto& set_bonus : equipment.set_bonuses)
                {
                    if (!is_valid_set_bonus(set_bonus))
                    {
                        return false;
                    }
                }

                return true;
            }

            [[nodiscard]] constexpr auto is_quest_item(Kind kind) -> bool
            {
                return kind == Kind::quest;
            }

            [[nodiscard]] constexpr auto is_collectible(Kind kind) -> bool
            {
                return kind == Kind::collectible;
            }

            [[nodiscard]] inline auto make_material_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::material;
                definition.stack.stackable = true;
                definition.stack.max_stack = 999;
                definition.binding.droppable = true;
                definition.binding.tradable = true;
                definition.binding.discardable = true;
                definition.binding.deletable = true;
                definition.material = MaterialDefinition{};
                return definition;
            }

            [[nodiscard]] inline auto make_consumable_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::consumable;
                definition.stack.stackable = true;
                definition.stack.max_stack = 99;
                definition.binding.droppable = true;
                definition.binding.tradable = true;
                definition.binding.discardable = true;
                definition.binding.deletable = true;
                definition.consumable = ConsumableDefinition{};
                return definition;
            }

            [[nodiscard]] inline auto make_equipment_definition(
                EquipmentClass equipment_class,
                EquipmentSlot slot,
                bool two_handed = false) -> Definition
            {
                Definition definition{};
                definition.kind = Kind::equipment;
                definition.stack.stackable = false;
                definition.stack.max_stack = 1;
                definition.binding.droppable = true;
                definition.binding.tradable = true;
                definition.binding.discardable = true;
                definition.binding.deletable = true;
                definition.equipment = EquipmentDefinition{};
                definition.equipment->equipment_class = equipment_class;
                definition.equipment->slot = slot;
                definition.equipment->two_handed = two_handed;
                definition.equipment->max_durability = 1;
                return definition;
            }

            [[nodiscard]] inline auto make_quest_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::quest;
                definition.stack.stackable = false;
                definition.stack.max_stack = 1;
                definition.binding.droppable = false;
                definition.binding.tradable = false;
                definition.binding.discardable = false;
                definition.binding.deletable = false;
                definition.quest = QuestDefinition{};
                return definition;
            }

            [[nodiscard]] inline auto make_collectible_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::collectible;
                definition.stack.stackable = false;
                definition.stack.max_stack = 1;
                definition.binding.droppable = true;
                definition.binding.tradable = true;
                definition.binding.discardable = true;
                definition.binding.deletable = true;
                definition.collectible = CollectibleDefinition{};
                return definition;
            }

            [[nodiscard]] inline auto make_instance(
                const Definition& definition,
                time::TimePoint now,
                std::uint32_t quantity = 1,
                std::optional<id::EntityId> owner_entity_id = {}) -> Instance
            {
                Instance instance{};
                instance.item_template_id = definition.identity.item_template_id;
                instance.owner_entity_id = owner_entity_id;
                instance.acquired_at = now;
                instance.bound = !definition.binding.droppable || !definition.binding.tradable;

                if (definition.stack.stackable)
                {
                    const auto max_stack = std::max<std::uint32_t>(1, definition.stack.max_stack);
                    instance.quantity = std::clamp(quantity, 1u, max_stack);
                }
                else
                {
                    instance.quantity = 1;
                }

                if (definition.equipment.has_value())
                {
                    instance.durability = definition.equipment->max_durability;
                }

                if (definition.quest.has_value())
                {
                    instance.quest_locked = definition.quest->locked_until_completion;
                    instance.bound = true;
                }

                return instance;
            }
        }
    }
}