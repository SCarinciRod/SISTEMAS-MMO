#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <limits>
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

            struct EquipmentRequirements
            {
                stat::Primary minimum_primary{};
                std::int32_t under_requirement_penalty_percent{ 0 };
            };

            struct EquipmentModifierRule
            {
                std::string_view effect_key{};
                std::uint32_t min_tier{ 1 };
                std::uint32_t max_tier{ 1 };
                status::Modifiers modifiers{};
            };

            struct EquipmentAffixInstance
            {
                std::string_view effect_key{};
                std::uint32_t tier{ 1 };
                status::Modifiers modifiers{};
            };

            [[nodiscard]] inline auto scale_signed(std::int32_t value, std::uint32_t tier, std::uint32_t max_tier) -> std::int32_t
            {
                if (value == 0 || tier == 0 || max_tier == 0)
                {
                    return 0;
                }

                const auto safe_tier = std::min(tier, max_tier);
                return static_cast<std::int32_t>((static_cast<std::int64_t>(value) * safe_tier) / max_tier);
            }

            [[nodiscard]] inline auto scale_primary(const stat::Primary& value, std::uint32_t tier, std::uint32_t max_tier) -> stat::Primary
            {
                return stat::Primary{
                    scale_signed(value.vitality, tier, max_tier),
                    scale_signed(value.strength, tier, max_tier),
                    scale_signed(value.agility, tier, max_tier),
                    scale_signed(value.intellect, tier, max_tier),
                    scale_signed(value.faith, tier, max_tier),
                    scale_signed(value.dexterity, tier, max_tier),
                    scale_signed(value.luck, tier, max_tier)
                };
            }

            [[nodiscard]] inline auto scale_derived(const stat::Derived& value, std::uint32_t tier, std::uint32_t max_tier) -> stat::Derived
            {
                return stat::Derived{
                    scale_signed(value.max_hp, tier, max_tier),
                    scale_signed(value.max_mana, tier, max_tier),
                    scale_signed(value.attack, tier, max_tier),
                    scale_signed(value.attack_speed, tier, max_tier),
                    scale_signed(value.magic_attack, tier, max_tier),
                    scale_signed(value.cast_speed, tier, max_tier),
                    scale_signed(value.defense, tier, max_tier),
                    scale_signed(value.magic_defense, tier, max_tier),
                    scale_signed(value.crit_chance, tier, max_tier),
                    scale_signed(value.crit_resist, tier, max_tier),
                    scale_signed(value.magic_crit, tier, max_tier),
                    scale_signed(value.magic_crit_res, tier, max_tier),
                    scale_signed(value.move_speed, tier, max_tier),
                    scale_signed(value.max_weight, tier, max_tier)
                };
            }

            [[nodiscard]] inline auto scale_modifiers(const status::Modifiers& value, std::uint32_t tier, std::uint32_t max_tier) -> status::Modifiers
            {
                status::Modifiers scaled = value;
                scaled.primary_delta = scale_primary(value.primary_delta, tier, max_tier);
                scaled.derived_delta = scale_derived(value.derived_delta, tier, max_tier);
                scaled.health_delta_per_tick = scale_signed(value.health_delta_per_tick, tier, max_tier);
                scaled.mana_delta_per_tick = scale_signed(value.mana_delta_per_tick, tier, max_tier);
                scaled.max_hp_percent_delta = scale_signed(value.max_hp_percent_delta, tier, max_tier);
                scaled.max_mana_percent_delta = scale_signed(value.max_mana_percent_delta, tier, max_tier);
                scaled.attack_percent_delta = scale_signed(value.attack_percent_delta, tier, max_tier);
                scaled.attack_speed_percent_delta = scale_signed(value.attack_speed_percent_delta, tier, max_tier);
                scaled.magic_attack_percent_delta = scale_signed(value.magic_attack_percent_delta, tier, max_tier);
                scaled.cast_speed_percent_delta = scale_signed(value.cast_speed_percent_delta, tier, max_tier);
                scaled.defense_percent_delta = scale_signed(value.defense_percent_delta, tier, max_tier);
                scaled.magic_defense_percent_delta = scale_signed(value.magic_defense_percent_delta, tier, max_tier);
                scaled.crit_chance_percent_delta = scale_signed(value.crit_chance_percent_delta, tier, max_tier);
                scaled.crit_resist_percent_delta = scale_signed(value.crit_resist_percent_delta, tier, max_tier);
                scaled.magic_crit_percent_delta = scale_signed(value.magic_crit_percent_delta, tier, max_tier);
                scaled.magic_crit_res_percent_delta = scale_signed(value.magic_crit_res_percent_delta, tier, max_tier);
                scaled.move_speed_percent_delta = scale_signed(value.move_speed_percent_delta, tier, max_tier);
                scaled.healing_received_percent_delta = scale_signed(value.healing_received_percent_delta, tier, max_tier);
                scaled.natural_regen_percent_delta = scale_signed(value.natural_regen_percent_delta, tier, max_tier);
                scaled.action_speed_percent_delta = scale_signed(value.action_speed_percent_delta, tier, max_tier);
                scaled.recovery_time_percent_delta = scale_signed(value.recovery_time_percent_delta, tier, max_tier);
                scaled.incoming_damage_percent_delta = scale_signed(value.incoming_damage_percent_delta, tier, max_tier);
                scaled.incoming_physical_damage_percent_delta = scale_signed(value.incoming_physical_damage_percent_delta, tier, max_tier);
                scaled.incoming_magical_damage_percent_delta = scale_signed(value.incoming_magical_damage_percent_delta, tier, max_tier);
                scaled.incoming_build_up_percent_delta = scale_signed(value.incoming_build_up_percent_delta, tier, max_tier);
                scaled.build_up_threshold_percent_delta = scale_signed(value.build_up_threshold_percent_delta, tier, max_tier);
                scaled.build_up_decay_per_second_percent_delta = scale_signed(value.build_up_decay_per_second_percent_delta, tier, max_tier);
                scaled.positive_effect_percent_delta = scale_signed(value.positive_effect_percent_delta, tier, max_tier);
                scaled.break_damage_bonus_percent = scale_signed(value.break_damage_bonus_percent, tier, max_tier);
                return scaled;
            }

            [[nodiscard]] inline auto make_equipment_affix_instance(const EquipmentModifierRule& rule, std::uint32_t tier) -> EquipmentAffixInstance
            {
                EquipmentAffixInstance instance{};
                instance.effect_key = rule.effect_key;
                instance.tier = std::clamp(tier, rule.min_tier, rule.max_tier);
                instance.modifiers = scale_modifiers(rule.modifiers, instance.tier, rule.max_tier);
                return instance;
            }

            [[nodiscard]] inline auto aggregate_equipment_affixes(const std::vector<EquipmentAffixInstance>& affixes) -> status::Modifiers
            {
                status::Modifiers modifiers{};
                for (const auto& affix : affixes)
                {
                    modifiers = status::add_modifiers(modifiers, affix.modifiers);
                }

                return modifiers;
            }

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
                std::int32_t attack{ 0 };
                std::int32_t defense{ 0 };
                EquipmentRequirements requirements{};
                std::uint32_t weight{ 0 };
                std::uint32_t max_durability{ 0 };
                bool two_handed{ false };
                EffectProfile effect{};
                std::uint32_t modification_slots{ 0 };
                std::uint32_t affix_slots{ 0 };
                std::vector<EquipmentModifierRule> modifier_pool{};
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
                std::vector<EquipmentAffixInstance> affixes{};
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

                switch (equipment.equipment_class)
                {
                    case EquipmentClass::weapon:
                        if (equipment.attack <= 0)
                        {
                            return false;
                        }
                        break;
                    case EquipmentClass::armor:
                        if (equipment.defense <= 0)
                        {
                            return false;
                        }
                        break;
                    default:
                        break;
                }

                if (equipment.affix_slots > 3)
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

                for (const auto& modifier_rule : equipment.modifier_pool)
                {
                    if (modifier_rule.effect_key.empty() || modifier_rule.min_tier == 0 || modifier_rule.max_tier < modifier_rule.min_tier)
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
                    instance.affixes.reserve(definition.equipment->affix_slots);
                }

                if (definition.quest.has_value())
                {
                    instance.quest_locked = definition.quest->locked_until_completion;
                    instance.bound = true;
                }

                return instance;
            }

            [[nodiscard]] inline auto calculate_item_weight(const Catalog& catalog, const Instance& instance) -> std::uint32_t
            {
                if (instance.quantity == 0)
                {
                    return 0;
                }

                const auto* definition = catalog.find(instance.item_template_id);
                if (definition == nullptr || !definition->equipment.has_value())
                {
                    return 0;
                }

                const auto total_weight = static_cast<std::uint64_t>(definition->equipment->weight) * instance.quantity;
                return static_cast<std::uint32_t>(std::min<std::uint64_t>(total_weight, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
            }

            [[nodiscard]] inline auto calculate_inventory_weight(const Catalog& catalog, const std::vector<Instance>& items) -> std::uint32_t
            {
                std::uint64_t total_weight = 0;
                for (const auto& item_instance : items)
                {
                    total_weight += calculate_item_weight(catalog, item_instance);
                }

                return static_cast<std::uint32_t>(std::min<std::uint64_t>(total_weight, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
            }
        }
    }
}