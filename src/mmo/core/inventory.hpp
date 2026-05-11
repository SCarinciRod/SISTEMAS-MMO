#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "id.hpp"
#include "item.hpp"

namespace mmo
{
    namespace core
    {
        namespace inventory
        {
            // Inventory contract: limits, instances, and validation outcomes.
            struct Limits
            {
                std::uint32_t max_slots{ 100 };
                std::uint32_t max_weight{ std::numeric_limits<std::uint32_t>::max() };
                bool enforce_slots{ true };
                bool enforce_weight{ false };
            };

            struct Inventory
            {
                std::vector<item::Instance> items{};
                Limits limits{};
            };

            enum class AddStatus : std::uint8_t
            {
                added = 0,
                stacked,
                partially_added,
                full,
                overweight,
                invalid_item,
                invalid_template,
                duplicated_item_id
            };

            enum class RemoveStatus : std::uint8_t
            {
                removed = 0,
                partially_removed,
                not_found,
                invalid_quantity,
                cannot_remove_equipped
            };

            enum class ValidationIssue : std::uint8_t
            {
                none = 0,
                invalid_item_id,
                invalid_template,
                duplicated_item_id,
                invalid_quantity,
                stack_exceeds_max,
                equipment_quantity_not_one,
                equipped_non_equipment,
                slot_limit_exceeded,
                weight_limit_exceeded
            };

            struct AddResult
            {
                AddStatus status{ AddStatus::invalid_item };
                std::uint32_t quantity_added{ 0 };
                std::uint32_t quantity_remaining{ 0 };
            };

            struct RemoveResult
            {
                RemoveStatus status{ RemoveStatus::not_found };
                std::uint32_t quantity_removed{ 0 };
                std::uint32_t quantity_remaining{ 0 };
            };

            [[nodiscard]] inline auto add_status_name(AddStatus status) -> std::string_view
            {
                switch (status)
                {
                    case AddStatus::added:
                        return "added";
                    case AddStatus::stacked:
                        return "stacked";
                    case AddStatus::partially_added:
                        return "partially_added";
                    case AddStatus::full:
                        return "full";
                    case AddStatus::overweight:
                        return "overweight";
                    case AddStatus::invalid_item:
                        return "invalid_item";
                    case AddStatus::invalid_template:
                        return "invalid_template";
                    case AddStatus::duplicated_item_id:
                        return "duplicated_item_id";
                    default:
                        return "unknown";
                }
            }

            [[nodiscard]] inline auto remove_status_name(RemoveStatus status) -> std::string_view
            {
                switch (status)
                {
                    case RemoveStatus::removed:
                        return "removed";
                    case RemoveStatus::partially_removed:
                        return "partially_removed";
                    case RemoveStatus::not_found:
                        return "not_found";
                    case RemoveStatus::invalid_quantity:
                        return "invalid_quantity";
                    case RemoveStatus::cannot_remove_equipped:
                        return "cannot_remove_equipped";
                    default:
                        return "unknown";
                }
            }

            [[nodiscard]] inline auto validation_issue_name(ValidationIssue issue) -> std::string_view
            {
                switch (issue)
                {
                    case ValidationIssue::none:
                        return "none";
                    case ValidationIssue::invalid_item_id:
                        return "invalid_item_id";
                    case ValidationIssue::invalid_template:
                        return "invalid_template";
                    case ValidationIssue::duplicated_item_id:
                        return "duplicated_item_id";
                    case ValidationIssue::invalid_quantity:
                        return "invalid_quantity";
                    case ValidationIssue::stack_exceeds_max:
                        return "stack_exceeds_max";
                    case ValidationIssue::equipment_quantity_not_one:
                        return "equipment_quantity_not_one";
                    case ValidationIssue::equipped_non_equipment:
                        return "equipped_non_equipment";
                    case ValidationIssue::slot_limit_exceeded:
                        return "slot_limit_exceeded";
                    case ValidationIssue::weight_limit_exceeded:
                        return "weight_limit_exceeded";
                    default:
                        return "unknown";
                }
            }

            [[nodiscard]] inline auto contains_item_id(
                const Inventory& inventory,
                id::ItemId item_id) -> bool
            {
                if (item_id == id::invalid_item_id)
                {
                    return false;
                }

                for (const auto& instance : inventory.items)
                {
                    if (instance.item_id == item_id)
                    {
                        return true;
                    }
                }

                return false;
            }

            [[nodiscard]] inline auto find_item(
                Inventory& inventory,
                id::ItemId item_id) -> item::Instance*
            {
                if (item_id == id::invalid_item_id)
                {
                    return nullptr;
                }

                for (auto& instance : inventory.items)
                {
                    if (instance.item_id == item_id)
                    {
                        return &instance;
                    }
                }

                return nullptr;
            }

            [[nodiscard]] inline auto find_item(
                const Inventory& inventory,
                id::ItemId item_id) -> const item::Instance*
            {
                if (item_id == id::invalid_item_id)
                {
                    return nullptr;
                }

                for (const auto& instance : inventory.items)
                {
                    if (instance.item_id == item_id)
                    {
                        return &instance;
                    }
                }

                return nullptr;
            }

            [[nodiscard]] inline auto has_duplicate_item_ids(const Inventory& inventory) -> bool
            {
                std::unordered_set<std::uint64_t> seen{};
                seen.reserve(inventory.items.size());

                for (const auto& instance : inventory.items)
                {
                    if (instance.item_id == id::invalid_item_id)
                    {
                        continue;
                    }

                    const auto raw_item_id = static_cast<std::uint64_t>(instance.item_id);

                    if (!seen.insert(raw_item_id).second)
                    {
                        return true;
                    }
                }

                return false;
            }

            [[nodiscard]] inline auto count_slots(const Inventory& inventory) -> std::uint32_t
            {
                return static_cast<std::uint32_t>(
                    std::min<std::size_t>(
                        inventory.items.size(),
                        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())));
            }

            [[nodiscard]] inline auto count_template_quantity(
                const Inventory& inventory,
                id::ItemTemplateId item_template_id) -> std::uint32_t
            {
                std::uint64_t total = 0;

                for (const auto& instance : inventory.items)
                {
                    if (instance.item_template_id == item_template_id)
                    {
                        total += instance.quantity;
                    }
                }

                return static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(
                        total,
                        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())));
            }

            // Weight and capacity calculations derived from item definitions.
            [[nodiscard]] inline auto calculate_weight(
                const item::Catalog& catalog,
                const Inventory& inventory) -> std::uint32_t
            {
                return item::calculate_inventory_weight(catalog, inventory.items);
            }

            [[nodiscard]] inline auto has_slot_capacity_for_new_slot(const Inventory& inventory) -> bool
            {
                if (!inventory.limits.enforce_slots)
                {
                    return true;
                }

                return inventory.items.size() < static_cast<std::size_t>(inventory.limits.max_slots);
            }

            [[nodiscard]] inline auto is_weight_within_limits(
                const item::Catalog& catalog,
                const Inventory& inventory) -> bool
            {
                if (!inventory.limits.enforce_weight)
                {
                    return true;
                }

                return calculate_weight(catalog, inventory) <= inventory.limits.max_weight;
            }

            [[nodiscard]] inline auto can_stack_together(
                const item::Definition& definition,
                const item::Instance& existing,
                const item::Instance& incoming) -> bool
            {
                if (!definition.stack.stackable)
                {
                    return false;
                }

                if (existing.item_template_id != incoming.item_template_id)
                {
                    return false;
                }

                if (existing.equipped || incoming.equipped)
                {
                    return false;
                }

                if (existing.bound != incoming.bound)
                {
                    return false;
                }

                if (existing.quest_locked != incoming.quest_locked)
                {
                    return false;
                }

                if (existing.owner_entity_id != incoming.owner_entity_id)
                {
                    return false;
                }

                const auto max_stack = std::max<std::uint32_t>(1, definition.stack.max_stack);

                return existing.quantity < max_stack;
            }

            // Validation rules for instances (stack sizes, equipment flags, IDs).
            [[nodiscard]] inline auto validate_instance(
                const item::Catalog& catalog,
                const item::Instance& instance) -> ValidationIssue
            {
                if (instance.item_id == id::invalid_item_id)
                {
                    return ValidationIssue::invalid_item_id;
                }

                if (instance.quantity == 0)
                {
                    return ValidationIssue::invalid_quantity;
                }

                const auto* definition = catalog.find(instance.item_template_id);

                if (definition == nullptr)
                {
                    return ValidationIssue::invalid_template;
                }

                if (definition->equipment.has_value())
                {
                    if (instance.quantity != 1)
                    {
                        return ValidationIssue::equipment_quantity_not_one;
                    }
                }
                else if (instance.equipped)
                {
                    return ValidationIssue::equipped_non_equipment;
                }

                if (definition->stack.stackable)
                {
                    const auto max_stack = std::max<std::uint32_t>(1, definition->stack.max_stack);

                    if (instance.quantity > max_stack)
                    {
                        return ValidationIssue::stack_exceeds_max;
                    }
                }
                else if (instance.quantity != 1)
                {
                    return ValidationIssue::invalid_quantity;
                }

                return ValidationIssue::none;
            }

            // Full inventory validation (limits, duplicates, per-item rules).
            [[nodiscard]] inline auto validate(
                const item::Catalog& catalog,
                const Inventory& inventory) -> ValidationIssue
            {
                if (inventory.limits.enforce_slots &&
                    inventory.items.size() > static_cast<std::size_t>(inventory.limits.max_slots))
                {
                    return ValidationIssue::slot_limit_exceeded;
                }

                if (!is_weight_within_limits(catalog, inventory))
                {
                    return ValidationIssue::weight_limit_exceeded;
                }

                std::unordered_set<std::uint64_t> seen{};
                seen.reserve(inventory.items.size());

                for (const auto& instance : inventory.items)
                {
                    const auto issue = validate_instance(catalog, instance);

                    if (issue != ValidationIssue::none)
                    {
                        return issue;
                    }

                    const auto raw_item_id = static_cast<std::uint64_t>(instance.item_id);

                    if (!seen.insert(raw_item_id).second)
                    {
                        return ValidationIssue::duplicated_item_id;
                    }
                }

                return ValidationIssue::none;
            }

            // Add/stack rules apply slot, weight, and template constraints.
            inline auto add_item(
                const item::Catalog& catalog,
                Inventory& inventory,
                item::Instance incoming) -> AddResult
            {
                AddResult result{};
                result.quantity_remaining = incoming.quantity;

                if (incoming.item_id == id::invalid_item_id || incoming.quantity == 0)
                {
                    result.status = AddStatus::invalid_item;
                    return result;
                }

                const auto* definition = catalog.find(incoming.item_template_id);

                if (definition == nullptr)
                {
                    result.status = AddStatus::invalid_template;
                    return result;
                }

                Inventory candidate = inventory;

                if (!definition->stack.stackable)
                {
                    incoming.quantity = 1;

                    if (contains_item_id(candidate, incoming.item_id))
                    {
                        result.status = AddStatus::duplicated_item_id;
                        result.quantity_remaining = incoming.quantity;
                        return result;
                    }

                    if (!has_slot_capacity_for_new_slot(candidate))
                    {
                        result.status = AddStatus::full;
                        result.quantity_remaining = incoming.quantity;
                        return result;
                    }

                    candidate.items.push_back(incoming);

                    const auto validation = validate(catalog, candidate);

                    if (validation == ValidationIssue::weight_limit_exceeded)
                    {
                        result.status = AddStatus::overweight;
                        result.quantity_remaining = incoming.quantity;
                        return result;
                    }

                    if (validation == ValidationIssue::slot_limit_exceeded)
                    {
                        result.status = AddStatus::full;
                        result.quantity_remaining = incoming.quantity;
                        return result;
                    }

                    if (validation != ValidationIssue::none)
                    {
                        result.status = AddStatus::invalid_item;
                        result.quantity_remaining = incoming.quantity;
                        return result;
                    }

                    inventory = std::move(candidate);

                    result.status = AddStatus::added;
                    result.quantity_added = 1;
                    result.quantity_remaining = 0;
                    return result;
                }

                const auto max_stack = std::max<std::uint32_t>(1, definition->stack.max_stack);

                std::uint32_t remaining = incoming.quantity;
                std::uint32_t added = 0;

                for (auto& existing : candidate.items)
                {
                    if (remaining == 0)
                    {
                        break;
                    }

                    if (!can_stack_together(*definition, existing, incoming))
                    {
                        continue;
                    }

                    const auto capacity = max_stack - existing.quantity;
                    const auto moved = std::min(capacity, remaining);

                    existing.quantity += moved;
                    remaining -= moved;
                    added += moved;
                }

                if (remaining > 0)
                {
                    if (contains_item_id(candidate, incoming.item_id))
                    {
                        result.status = added > 0 ? AddStatus::partially_added : AddStatus::duplicated_item_id;
                        result.quantity_added = added;
                        result.quantity_remaining = remaining;
                        return result;
                    }

                    if (has_slot_capacity_for_new_slot(candidate))
                    {
                        auto new_stack = incoming;
                        new_stack.quantity = std::min(max_stack, remaining);

                        candidate.items.push_back(new_stack);

                        remaining -= new_stack.quantity;
                        added += new_stack.quantity;
                    }
                }

                const auto validation = validate(catalog, candidate);

                if (validation == ValidationIssue::weight_limit_exceeded)
                {
                    result.status = added > 0 ? AddStatus::partially_added : AddStatus::overweight;
                    result.quantity_added = 0;
                    result.quantity_remaining = incoming.quantity;
                    return result;
                }

                if (validation == ValidationIssue::slot_limit_exceeded)
                {
                    result.status = added > 0 ? AddStatus::partially_added : AddStatus::full;
                    result.quantity_added = 0;
                    result.quantity_remaining = incoming.quantity;
                    return result;
                }

                if (validation != ValidationIssue::none)
                {
                    result.status = AddStatus::invalid_item;
                    result.quantity_added = 0;
                    result.quantity_remaining = incoming.quantity;
                    return result;
                }

                inventory = std::move(candidate);

                if (added == 0)
                {
                    result.status = AddStatus::full;
                    result.quantity_added = 0;
                    result.quantity_remaining = remaining;
                    return result;
                }

                result.status = remaining == 0 ? AddStatus::added : AddStatus::partially_added;
                result.quantity_added = added;
                result.quantity_remaining = remaining;

                if (result.status == AddStatus::added && added == incoming.quantity)
                {
                    bool only_stacked = true;

                    for (const auto& instance : inventory.items)
                    {
                        if (instance.item_id == incoming.item_id)
                        {
                            only_stacked = false;
                            break;
                        }
                    }

                    if (only_stacked)
                    {
                        result.status = AddStatus::stacked;
                    }
                }

                return result;
            }

            [[nodiscard]] inline auto would_fit(
                const item::Catalog& catalog,
                const Inventory& inventory,
                item::Instance incoming) -> bool
            {
                Inventory candidate = inventory;
                const auto result = add_item(catalog, candidate, incoming);

                return result.status == AddStatus::added ||
                       result.status == AddStatus::stacked ||
                       result.status == AddStatus::partially_added;
            }

            // Removal rules handle equipment locks and partial stack reductions.
            inline auto remove_item_by_id(
                Inventory& inventory,
                id::ItemId item_id,
                std::uint32_t quantity = 1,
                bool allow_equipped = false) -> RemoveResult
            {
                RemoveResult result{};

                if (quantity == 0)
                {
                    result.status = RemoveStatus::invalid_quantity;
                    return result;
                }

                for (auto it = inventory.items.begin(); it != inventory.items.end(); ++it)
                {
                    if (it->item_id != item_id)
                    {
                        continue;
                    }

                    if (it->equipped && !allow_equipped)
                    {
                        result.status = RemoveStatus::cannot_remove_equipped;
                        result.quantity_remaining = it->quantity;
                        return result;
                    }

                    const auto removed = std::min(quantity, it->quantity);

                    it->quantity -= removed;

                    result.quantity_removed = removed;
                    result.quantity_remaining = it->quantity;

                    if (it->quantity == 0)
                    {
                        inventory.items.erase(it);
                        result.status = RemoveStatus::removed;
                    }
                    else
                    {
                        result.status = RemoveStatus::partially_removed;
                    }

                    return result;
                }

                result.status = RemoveStatus::not_found;
                return result;
            }

            inline auto clear(Inventory& inventory) -> void
            {
                inventory.items.clear();
            }

            [[nodiscard]] inline auto empty(const Inventory& inventory) -> bool
            {
                return inventory.items.empty();
            }

            [[nodiscard]] inline auto size(const Inventory& inventory) -> std::size_t
            {
                return inventory.items.size();
            }
        }
    }
}