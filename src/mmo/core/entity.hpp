#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include "experience.hpp"
#include "item.hpp"
#include "inventory.hpp"
#include "numeric.hpp"
#include "stat.hpp"
#include "status.hpp"
#include "time.hpp"
#include <string_view>
#include <set>
#include <vector>
#include "id.hpp"
#include "action.hpp"
#include "combat.hpp"
#include "combat_damage.hpp"


namespace mmo
{
    namespace world
    {
        class World;
    }

    namespace core
    {
        namespace entity
        {
            // Entity runtime contract: definitions for records and state tracking.
            enum class Type : std::uint8_t
            {
                unknown = 0,
                player,
                npc,
                monster,
                item,
                projectile,
                system
            };

            struct Blueprint
            {
                id::EntityTemplateId template_id{ id::invalid_entity_template_id };
                id::SpeciesId species_id{ id::invalid_species_id };
                Type type{ Type::unknown };
                std::string_view display_name{};
                stat::Primary base_primary{};
                std::uint32_t level{ 1 };
                std::uint64_t experience_reward{ 0 };
            };

            struct Identity
            {
                id::EntityId entity_id{ id::invalid_entity_id };
                id::EntityTemplateId template_id{ id::invalid_entity_template_id };
                id::SpeciesId species_id{ id::invalid_species_id };
                Type type{ Type::unknown };
            };

            class Placement
            {
            public:
                Placement() = default;
                Placement(const Placement&) = default;
                Placement(Placement&&) noexcept = default;

                [[nodiscard]] auto zone_id() const noexcept -> id::ZoneId
                {
                    return zone_id_;
                }

            private:
                auto operator=(const Placement&) -> Placement& = default;
                auto operator=(Placement&&) noexcept -> Placement& = default;

                auto set_zone(id::ZoneId zone_id) noexcept -> void
                {
                    zone_id_ = zone_id;
                }

                id::ZoneId zone_id_{ id::invalid_zone_id };

                friend class Table;
            };

            struct Stats
            {
                stat::Primary base_primary{};
                stat::Primary current_primary{};
                stat::Derived current_derived{};
            };

            struct Resources
            {
                std::int32_t health_current{ 0 };
                std::int32_t mana_current{ 0 };
                std::int32_t shield_current{ 0 };
            };

            struct Progression
            {
                std::uint32_t level{ 1 };
                std::uint64_t experience{ 0 };
                std::uint64_t experience_to_next{ 0 };
            };

            using Inventory = mmo::core::inventory::Inventory;

            struct Load
            {
                std::uint32_t carried_weight{ 0 };
                stat::Encumbrance encumbrance{};
                bool inventory_dirty{ true };
            };

            // Combat state: status stacks, action timing, aggro, and damage ledgers.
            struct Combat
            {
                status::Table status_effects{};
                action::State action_state{};
                std::optional<time::TimePoint> action_ready_at{};
                combat::AggroState aggro{};

                damage::Ledger damage_taken{};
                damage::Ledger damage_dealt{};

                bool guaranteed_critical_hit{ false };
                bool negative_status_immunity{ false };
            };

            // Lifecycle tracking for spawn, damage, and kill attribution.
            struct Lifecycle
            {
                time::TimePoint spawned_at{};
                std::optional<time::TimePoint> last_damage_at{};
                std::optional<time::TimePoint> death_at{};

                bool alive{ true };

                std::optional<id::EntityId> killer_entity_id{};
                damage::Source killing_source{};
                std::optional<damage::Trace> killing_trace{};
            };

            struct Contract
            {
                Identity identity{};
                Placement placement{};
                Stats stats{};
                Resources resources{};
                Progression progression{};
                Inventory inventory{};
                Load load{};
                Combat combat{};
                Lifecycle lifecycle{};
            };

            using Record = Contract;

            struct PeriodicStatusResult
            {
                bool entity_found{ false };
                std::uint64_t applications{ 0 };
            };

            // Load/encumbrance calculations derived from inventory weight.
            [[nodiscard]] inline auto calculate_carried_weight(
                const item::Catalog& catalog,
                const Inventory& inventory) -> std::uint32_t
            {
                return mmo::core::inventory::calculate_weight(catalog, inventory);
            }

            inline auto sync_load(Record& record, const item::Catalog& catalog) -> void
            {
                record.load.carried_weight = calculate_carried_weight(catalog, record.inventory);

                record.load.encumbrance = stat::derive_encumbrance(
                    record.load.carried_weight,
                    record.stats.current_derived.max_weight);
                record.load.inventory_dirty = false;
            }

            inline auto mark_inventory_load_dirty(Record& record) noexcept -> void
            {
                record.load.inventory_dirty = true;
            }

            [[nodiscard]] inline auto validate_inventory(
                const item::Catalog& catalog,
                const Inventory& inventory) -> mmo::core::inventory::ValidationIssue
            {
                return mmo::core::inventory::validate(catalog, inventory);
            }

            inline auto add_inventory_item(
                Record& record,
                const item::Catalog& catalog,
                item::Instance instance) -> mmo::core::inventory::AddResult
            {
                auto result = mmo::core::inventory::add_item(
                    catalog,
                    record.inventory,
                    instance);

                if (result.quantity_added > 0)
                {
                    mark_inventory_load_dirty(record);
                    sync_load(record, catalog);
                }

                return result;
            }

            inline auto remove_inventory_item_by_id(
                Record& record,
                const item::Catalog& catalog,
                id::ItemId item_id,
                std::uint32_t quantity = 1,
                bool allow_equipped = false) -> mmo::core::inventory::RemoveResult
            {
                auto result = mmo::core::inventory::remove_item_by_id(
                    record.inventory,
                    item_id,
                    quantity,
                    allow_equipped);

                if (result.quantity_removed > 0)
                {
                    mark_inventory_load_dirty(record);
                    sync_load(record, catalog);
                }

                return result;
            }

            // Defense profile used by damage resolution.
            [[nodiscard]] inline auto make_defense_profile(const Record& record) -> mmo::core::combat::DefenseProfile
            {
                mmo::core::combat::DefenseProfile defense{};

                defense.physical_reduction_percent = 0;
                defense.magical_reduction_percent = 0;
                defense.flat_reduction = 0;
                defense.shield_current = record.resources.shield_current;

                return defense;
            }

            // Experience reward fallback if no explicit reward is provided.
            [[nodiscard]] inline auto resolve_experience_reward(const Blueprint& blueprint) -> std::uint64_t
            {
                if (blueprint.experience_reward > 0)
                {
                    return blueprint.experience_reward;
                }

                return experience::reward_for_level(blueprint.level);
            }

            class Catalog
            {
            public:
                auto insert(const Blueprint& blueprint) -> bool
                {
                    auto insert_result = blueprints_.emplace(blueprint.template_id, blueprint);
                    auto& it = insert_result.first;
                    (void)it;

                    const bool inserted = insert_result.second;
                    if (!inserted)
                    {
                        return false;
                    }

                    species_index_[blueprint.species_id].push_back(blueprint.template_id);
                    return true;
                }

                auto erase(id::EntityTemplateId template_id) -> bool
                {
                    auto blueprint_it = blueprints_.find(template_id);
                    if (blueprint_it == blueprints_.end())
                    {
                        return false;
                    }

                    unlink_from_species(blueprint_it->second.species_id, template_id);
                    blueprints_.erase(blueprint_it);
                    return true;
                }

                [[nodiscard]] auto find(id::EntityTemplateId template_id) -> Blueprint*
                {
                    auto blueprint_it = blueprints_.find(template_id);
                    if (blueprint_it == blueprints_.end())
                    {
                        return nullptr;
                    }

                    return &blueprint_it->second;
                }

                [[nodiscard]] auto find(id::EntityTemplateId template_id) const -> const Blueprint*
                {
                    auto blueprint_it = blueprints_.find(template_id);
                    if (blueprint_it == blueprints_.end())
                    {
                        return nullptr;
                    }

                    return &blueprint_it->second;
                }

                [[nodiscard]] auto list_by_species(id::SpeciesId species_id) const -> std::vector<const Blueprint*>
                {
                    std::vector<const Blueprint*> blueprints;

                    auto species_it = species_index_.find(species_id);
                    if (species_it == species_index_.end())
                    {
                        return blueprints;
                    }

                    blueprints.reserve(species_it->second.size());

                    for (const auto template_id : species_it->second)
                    {
                        auto blueprint_it = blueprints_.find(template_id);
                        if (blueprint_it != blueprints_.end())
                        {
                            blueprints.push_back(&blueprint_it->second);
                        }
                    }

                    return blueprints;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return blueprints_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return blueprints_.empty();
                }

            private:
                std::map<id::EntityTemplateId, Blueprint> blueprints_;
                std::map<id::SpeciesId, std::vector<id::EntityTemplateId>> species_index_;

                auto unlink_from_species(id::SpeciesId species_id, id::EntityTemplateId template_id) -> void
                {
                    auto species_it = species_index_.find(species_id);
                    if (species_it == species_index_.end())
                    {
                        return;
                    }

                    auto& template_ids = species_it->second;

                    for (auto it = template_ids.begin(); it != template_ids.end(); ++it)
                    {
                        if (*it == template_id)
                        {
                            template_ids.erase(it);
                            break;
                        }
                    }

                    if (template_ids.empty())
                    {
                        species_index_.erase(species_it);
                    }
                }
            };

            class Table
            {
            private:
                friend class mmo::world::World;

                auto spawn(
                    id::EntityId entity_id,
                    const Blueprint& blueprint,
                    id::ZoneId zone_id,
                    time::TimePoint now) -> bool
                {
                    if (entity_id == id::invalid_entity_id || zone_id == id::invalid_zone_id)
                    {
                        return false;
                    }

                    Record record{};
                    record.identity.entity_id = entity_id;
                    record.identity.template_id = blueprint.template_id;
                    record.identity.species_id = blueprint.species_id;
                    record.identity.type = blueprint.type;
                    record.placement.set_zone(zone_id);
                    record.stats.base_primary = blueprint.base_primary;
                    record.stats.current_primary = blueprint.base_primary;
                    record.stats.current_derived = stat::derive(record.stats.current_primary);
                    record.resources.health_current = record.stats.current_derived.max_hp;
                    record.resources.mana_current = record.stats.current_derived.max_mana;
                    record.progression.level = std::max<std::uint32_t>(1, blueprint.level);
                    record.progression.experience_to_next = experience::required_for_level(record.progression.level);
                    record.lifecycle.spawned_at = now;
                    record.lifecycle.alive = true;

                    return insert(record);
                }

                auto insert(const Record& record) -> bool
                {
                    if (record.identity.entity_id == id::invalid_entity_id ||
                        record.placement.zone_id() == id::invalid_zone_id)
                    {
                        return false;
                    }

                    auto insert_result = records_.emplace(record.identity.entity_id, record);
                    auto& it = insert_result.first;

                    const bool inserted = insert_result.second;
                    if (!inserted)
                    {
                        return false;
                    }

                    try
                    {
                        refresh_record(it->second);
                        link_to_zone(record.placement.zone_id(), record.identity.entity_id);
                    }
                    catch (...)
                    {
                        records_.erase(it);
                        throw;
                    }
                    return true;
                }

                auto erase(id::EntityId entity_id) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    unlink_from_zone(record_it->second.placement.zone_id(), entity_id);
                    records_.erase(record_it);
                    return true;
                }

            public:
                [[nodiscard]] auto find(id::EntityId entity_id) const -> const Record*
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return nullptr;
                    }

                    return &record_it->second;
                }

            private:
                auto move_to_zone(id::EntityId entity_id, id::ZoneId zone_id) -> bool
                {
                    if (zone_id == id::invalid_zone_id)
                    {
                        return false;
                    }

                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    if (record_it->second.placement.zone_id() == zone_id)
                    {
                        return true;
                    }

                    // Allocate destination membership before the no-throw placement commit.
                    link_to_zone(zone_id, entity_id);
                    unlink_from_zone(record_it->second.placement.zone_id(), entity_id);
                    record_it->second.placement.set_zone(zone_id);
                    return true;
                }

            public:
                auto mark_damage(id::EntityId entity_id, time::TimePoint when) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.lifecycle.last_damage_at = when;
                    return true;
                }

                auto set_base_primary(id::EntityId entity_id, const stat::Primary& base_primary) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.stats.base_primary = base_primary;
                    refresh_record(record_it->second);
                    return true;
                }

                auto recalculate_stats(id::EntityId entity_id) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    refresh_record(record_it->second);
                    return true;
                }

                auto sync_inventory_load_if_dirty(
                    id::EntityId entity_id,
                    const item::Catalog& catalog) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end() || !record_it->second.load.inventory_dirty)
                    {
                        return false;
                    }

                    sync_load(record_it->second, catalog);
                    refresh_record(record_it->second);
                    return true;
                }

                auto add_inventory_item(
                    id::EntityId entity_id,
                    const item::Catalog& catalog,
                    item::Instance instance) -> mmo::core::inventory::AddResult
                {
                    auto record_it = records_.find(entity_id);

                    if (record_it == records_.end())
                    {
                        mmo::core::inventory::AddResult result{};
                        result.status = mmo::core::inventory::AddStatus::invalid_item;
                        result.quantity_remaining = instance.quantity;
                        return result;
                    }

                    // Status aggregation may allocate (shield layers); prepare it before inventory commit.
                    const auto totals = record_it->second.combat.status_effects.aggregate();
                    auto result = mmo::core::inventory::add_item(
                        catalog,
                        record_it->second.inventory,
                        instance);

                    if (result.quantity_added > 0)
                    {
                        mmo::core::entity::mark_inventory_load_dirty(record_it->second);
                        sync_load(record_it->second, catalog);
                        refresh_record(record_it->second, totals);
                    }

                    return result;
                }

                auto remove_inventory_item_by_id(
                    id::EntityId entity_id,
                    const item::Catalog& catalog,
                    id::ItemId item_id,
                    std::uint32_t quantity = 1,
                    bool allow_equipped = false) -> mmo::core::inventory::RemoveResult
                {
                    auto record_it = records_.find(entity_id);

                    if (record_it == records_.end())
                    {
                        mmo::core::inventory::RemoveResult result{};
                        result.status = mmo::core::inventory::RemoveStatus::not_found;
                        return result;
                    }

                    auto result = mmo::core::inventory::remove_item_by_id(
                        record_it->second.inventory,
                        item_id,
                        quantity,
                        allow_equipped);

                    if (result.quantity_removed > 0)
                    {
                        mmo::core::entity::mark_inventory_load_dirty(record_it->second);
                        sync_load(record_it->second, catalog);
                        refresh_record(record_it->second);
                    }

                    return result;
                }

                [[nodiscard]] auto validate_inventory(
                    id::EntityId entity_id,
                    const item::Catalog& catalog) const -> mmo::core::inventory::ValidationIssue
                {
                    auto record_it = records_.find(entity_id);

                    if (record_it == records_.end())
                    {
                        return mmo::core::inventory::ValidationIssue::invalid_item_id;
                    }

                    return mmo::core::inventory::validate(catalog, record_it->second.inventory);
                }

                // Damage pipeline: resolve, record traces, apply shields/HP, and set death metadata.
                auto apply_damage(
                    id::EntityId target_entity_id,
                    const mmo::core::combat::DamageProfile& profile,
                    const mmo::core::combat::DefenseProfile& defense,
                    time::TimePoint now) -> std::optional<mmo::core::combat::DamageResult>
                {
                    auto target_it = records_.find(target_entity_id);

                    if (target_it == records_.end())
                    {
                        return std::nullopt;
                    }

                    auto& target = target_it->second;
                    const bool was_alive = target.lifecycle.alive;

                    auto effective_defense = defense;
                    effective_defense.shield_current = target.resources.shield_current;

                    const auto result = mmo::core::combat::resolve_damage(
                        profile,
                        effective_defense);

                    const auto trace = mmo::core::combat::to_damage_trace(result);

                    target.combat.damage_taken.record(trace);
                    target.lifecycle.last_damage_at = now;

                    if (result.shield_absorbed > 0)
                    {
                        target.resources.shield_current = numeric::clamp_to_int32(
                            std::max<std::int64_t>(
                                0,
                                static_cast<std::int64_t>(target.resources.shield_current) -
                                    result.shield_absorbed));
                    }

                    if (result.shield_broken)
                    {
                        target.resources.shield_current = 0;
                        target.combat.status_effects.remove(status::Kind::shield);
                    }

                    if (result.health_damage > 0)
                    {
                        target.resources.health_current = numeric::clamp_to_int32(
                            std::clamp<std::int64_t>(
                                static_cast<std::int64_t>(target.resources.health_current) -
                                    result.health_damage,
                                0,
                                target.stats.current_derived.max_hp));
                    }

                    if (profile.packet.source.entity_id.has_value())
                    {
                        const auto source_entity_id = profile.packet.source.entity_id.value();
                        auto source_it = records_.find(source_entity_id);

                        if (source_it != records_.end())
                        {
                            source_it->second.combat.damage_dealt.record(trace);
                        }

                        if (result.health_damage > 0)
                        {
                            target.combat.aggro.add_threat(
                                source_entity_id,
                                static_cast<std::uint32_t>(result.health_damage),
                                now);
                        }
                    }

                    if (target.resources.health_current <= 0)
                    {
                        target.lifecycle.alive = false;

                        if (was_alive)
                        {
                            target.lifecycle.death_at = now;
                            target.lifecycle.killing_source = profile.packet.source;
                            target.lifecycle.killing_trace = trace;

                            if (profile.packet.source.entity_id.has_value())
                            {
                                target.lifecycle.killer_entity_id = profile.packet.source.entity_id.value();
                            }
                            else
                            {
                                target.lifecycle.killer_entity_id.reset();
                            }
                        }
                    }

                    if (result.shield_broken)
                    {
                        refresh_record(target);
                    }

                    return result;
                }

                auto apply_damage(
                    id::EntityId target_entity_id,
                    const mmo::core::combat::DamageProfile& profile,
                    time::TimePoint now) -> std::optional<mmo::core::combat::DamageResult>
                {
                    auto target_it = records_.find(target_entity_id);

                    if (target_it == records_.end())
                    {
                        return std::nullopt;
                    }

                    const auto defense = make_defense_profile(target_it->second);

                    return apply_damage(
                        target_entity_id,
                        profile,
                        defense,
                        now);
                }

                auto clear_damage_history(id::EntityId entity_id) -> bool
                {
                    auto record_it = records_.find(entity_id);

                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.combat.damage_taken.clear();
                    record_it->second.combat.damage_dealt.clear();

                    return true;
                }

                auto apply_status(id::EntityId entity_id, const status::Instance& instance) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    const auto active_totals = record_it->second.combat.status_effects.aggregate();
                    const auto active_modifiers = amplify_positive_effects(active_totals.modifiers);

                    if (status::is_negative_kind(instance.kind) && active_modifiers.negative_status_immunity)
                    {
                        return false;
                    }

                    record_it->second.combat.status_effects.apply(instance);

                    apply_status_side_effects(record_it->second, instance);

                    refresh_record(record_it->second);
                    return true;
                }

                auto set_action_state(
                    id::EntityId entity_id,
                    const action::State& action_state,
                    time::TimePoint ready_at) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.combat.action_state = action_state;
                    record_it->second.combat.action_ready_at = ready_at;
                    return true;
                }

                auto clear_action_state(id::EntityId entity_id) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.combat.action_state = action::State{};
                    record_it->second.combat.action_ready_at.reset();
                    return true;
                }

                auto add_threat(
                    id::EntityId entity_id,
                    id::EntityId source_entity_id,
                    std::uint32_t amount,
                    time::TimePoint now) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.combat.aggro.add_threat(source_entity_id, amount, now);
                    return true;
                }

                auto force_target(
                    id::EntityId entity_id,
                    id::EntityId target_entity_id,
                    time::Milliseconds duration,
                    time::TimePoint now) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.combat.aggro.force_target(target_entity_id, duration, now);
                    return true;
                }

                auto clear_forced_target(id::EntityId entity_id) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.combat.aggro.clear_forced_target();
                    return true;
                }

                [[nodiscard]] auto select_target(id::EntityId entity_id, time::TimePoint now) const -> std::optional<id::EntityId>
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return std::nullopt;
                    }

                    return record_it->second.combat.aggro.select_target(now);
                }

                auto accumulate_status(
                    id::EntityId entity_id,
                    const status::Definition& definition,
                    std::uint32_t amount,
                    time::TimePoint now,
                    std::optional<id::EntityId> source_entity_id = {},
                    std::optional<id::SpeciesId> source_species_id = {},
                    std::optional<id::EntityTemplateId> source_template_id = {}) -> std::optional<status::Instance>
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return std::nullopt;
                    }

                    const auto active_totals = record_it->second.combat.status_effects.aggregate();
                    const auto active_modifiers = amplify_positive_effects(active_totals.modifiers);

                    if (status::is_negative_kind(definition.kind) && active_modifiers.negative_status_immunity)
                    {
                        return std::nullopt;
                    }

                    if (definition.delivery == status::Delivery::build_up &&
                        definition.kind != status::Kind::curse &&
                        record_it->second.combat.status_effects.contains(status::Kind::shield))
                    {
                        return std::nullopt;
                    }

                    auto instance = record_it->second.combat.status_effects.accumulate(
                        definition,
                        amount,
                        now,
                        active_modifiers.build_up_threshold_percent_delta,
                        active_modifiers.build_up_decay_per_second_percent_delta,
                        source_entity_id,
                        source_species_id,
                        source_template_id);

                    if (instance.has_value())
                    {
                        apply_status_side_effects(record_it->second, instance.value());
                        refresh_record(record_it->second);
                    }

                    return instance;
                }

                [[nodiscard]] auto process_periodic_statuses(
                    id::EntityId entity_id,
                    time::TimePoint now,
                    std::uint64_t max_applications_per_status =
                        std::numeric_limits<std::uint64_t>::max()) -> PeriodicStatusResult
                {
                    PeriodicStatusResult result{};
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return result;
                    }

                    result.entity_found = true;
                    auto& record = record_it->second;
                    const auto totals = record.combat.status_effects.consume_periodic(
                        now,
                        max_applications_per_status);
                    result.applications = totals.applications;

                    auto remaining_health_delta = totals.health_delta;
                    while (remaining_health_delta != 0)
                    {
                        std::int64_t bounded_delta{};
                        if (remaining_health_delta > 0)
                        {
                            bounded_delta = std::min<std::int64_t>(
                                remaining_health_delta,
                                static_cast<std::int64_t>(record.stats.current_derived.max_hp) -
                                    record.resources.health_current);
                        }
                        else
                        {
                            const auto available =
                                static_cast<std::int64_t>(record.resources.health_current) +
                                record.resources.shield_current;
                            const auto requested_damage = remaining_health_delta ==
                                    std::numeric_limits<std::int64_t>::min()
                                ? std::numeric_limits<std::int64_t>::max()
                                : -remaining_health_delta;
                            const auto bounded_damage = std::min<std::int64_t>(
                                { requested_damage,
                                  available,
                                  std::numeric_limits<std::int32_t>::max() });
                            bounded_delta = -bounded_damage;
                        }

                        if (bounded_delta == 0)
                        {
                            break;
                        }

                        adjust_health_record(
                            record,
                            static_cast<std::int32_t>(bounded_delta),
                            now);
                        remaining_health_delta -= bounded_delta;
                    }

                    if (totals.mana_delta != 0)
                    {
                        const auto minimum_delta = -static_cast<std::int64_t>(
                            record.resources.mana_current);
                        const auto maximum_delta = static_cast<std::int64_t>(
                            record.stats.current_derived.max_mana) - record.resources.mana_current;
                        const auto bounded_delta = std::clamp(
                            totals.mana_delta,
                            minimum_delta,
                            maximum_delta);

                        record.resources.mana_current = numeric::clamp_to_int32(
                            static_cast<std::int64_t>(record.resources.mana_current) +
                                bounded_delta);
                    }

                    return result;
                }

                auto sweep_statuses(id::EntityId entity_id, time::TimePoint now) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    const auto active_totals = record_it->second.combat.status_effects.aggregate();
                    const auto active_modifiers = amplify_positive_effects(active_totals.modifiers);

                    const auto changed = record_it->second.combat.status_effects.sweep(
                        now,
                        active_modifiers.build_up_decay_per_second_percent_delta);

                    if (changed)
                    {
                        refresh_record(record_it->second);
                    }

                    return changed;
                }

                auto remove_status(id::EntityId entity_id, status::Kind kind) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    auto removed = record_it->second.combat.status_effects.remove(kind);

                    if (removed)
                    {
                        if (kind == status::Kind::taunt)
                        {
                            record_it->second.combat.aggro.clear_forced_target();
                        }

                        if (kind == status::Kind::shield)
                        {
                            record_it->second.resources.shield_current = 0;
                        }

                        refresh_record(record_it->second);
                    }

                    return removed;
                }

                auto purge_expired_statuses(id::EntityId entity_id, time::TimePoint now) -> std::vector<status::Instance>
                {
                    std::vector<status::Instance> removed_statuses;

                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return removed_statuses;
                    }

                    removed_statuses = record_it->second.combat.status_effects.remove_expired(now);

                    if (!removed_statuses.empty())
                    {
                        const auto taunt_expired = std::any_of(
                            removed_statuses.begin(),
                            removed_statuses.end(),
                            [](const status::Instance& removed_status) {
                                return removed_status.kind == status::Kind::taunt;
                            });

                        const auto shield_expired = std::any_of(
                            removed_statuses.begin(),
                            removed_statuses.end(),
                            [](const status::Instance& removed_status) {
                                return removed_status.kind == status::Kind::shield;
                            });

                        if (taunt_expired)
                        {
                            record_it->second.combat.aggro.clear_forced_target();
                        }

                        if (shield_expired)
                        {
                            record_it->second.resources.shield_current = 0;
                        }

                        refresh_record(record_it->second);
                    }

                    return removed_statuses;
                }

                // Raw health adjustments with shield handling and death flagging.
                auto adjust_health(
                    id::EntityId entity_id,
                    std::int32_t delta,
                    time::TimePoint simulation_time) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    adjust_health_record(record_it->second, delta, simulation_time);
                    return true;
                }

                auto adjust_mana(id::EntityId entity_id, std::int32_t delta) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.resources.mana_current = numeric::clamp_to_int32(
                        std::clamp<std::int64_t>(
                            static_cast<std::int64_t>(record_it->second.resources.mana_current) +
                                delta,
                            0,
                            record_it->second.stats.current_derived.max_mana));

                    return true;
                }

                // Experience gain uses the curve contract to advance levels.
                auto grant_experience(id::EntityId entity_id, std::uint64_t amount) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    if (record_it->second.identity.type != Type::player)
                    {
                        return false;
                    }

                    const auto result = experience::gain(
                        record_it->second.progression.level,
                        record_it->second.progression.experience,
                        record_it->second.progression.experience_to_next,
                        amount);

                    record_it->second.progression.level = result.level;
                    record_it->second.progression.experience = result.experience;
                    record_it->second.progression.experience_to_next = result.experience_to_next;

                    return result.levels_gained > 0;
                }

                [[nodiscard]] auto ids_in_zone(id::ZoneId zone_id) const -> std::vector<id::EntityId>
                {
                    std::vector<id::EntityId> entity_ids;

                    auto zone_it = zone_index_.find(zone_id);
                    if (zone_it == zone_index_.end())
                    {
                        return entity_ids;
                    }

                    entity_ids.reserve(zone_it->second.size());

                    for (const auto entity_id : zone_it->second)
                    {
                        entity_ids.push_back(entity_id);
                    }

                    return entity_ids;
                }

                [[nodiscard]] auto records_in_zone(id::ZoneId zone_id) const -> std::vector<const Record*>
                {
                    std::vector<const Record*> records;

                    auto zone_it = zone_index_.find(zone_id);
                    if (zone_it == zone_index_.end())
                    {
                        return records;
                    }

                    records.reserve(zone_it->second.size());

                    for (const auto entity_id : zone_it->second)
                    {
                        auto record_it = records_.find(entity_id);
                        if (record_it != records_.end())
                        {
                            records.push_back(&record_it->second);
                        }
                    }

                    return records;
                }

                [[nodiscard]] auto list_ids() const -> std::vector<id::EntityId>
                {
                    std::vector<id::EntityId> ids;
                    ids.reserve(records_.size());

                    for (const auto& entry : records_)
                    {
                        ids.push_back(entry.first);
                    }

                    return ids;
                }

                [[nodiscard]] auto has_consistent_indexes() const -> bool
                {
                    std::size_t indexed_entities = 0;

                    for (const auto& zone_entry : zone_index_)
                    {
                        for (const auto entity_id : zone_entry.second)
                        {
                            const auto record_it = records_.find(entity_id);
                            if (record_it == records_.end() ||
                                record_it->second.identity.entity_id != entity_id ||
                                record_it->second.placement.zone_id() != zone_entry.first)
                            {
                                return false;
                            }

                            ++indexed_entities;
                        }
                    }

                    if (indexed_entities != records_.size())
                    {
                        return false;
                    }

                    for (const auto& record_entry : records_)
                    {
                        if (record_entry.first != record_entry.second.identity.entity_id)
                        {
                            return false;
                        }

                        const auto zone_it = zone_index_.find(
                            record_entry.second.placement.zone_id());
                        if (zone_it == zone_index_.end() ||
                            zone_it->second.count(record_entry.first) != 1)
                        {
                            return false;
                        }
                    }

                    return true;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return records_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return records_.empty();
                }

            private:
                std::map<id::EntityId, Record> records_;
                std::map<id::ZoneId, std::set<id::EntityId>> zone_index_;

                auto adjust_health_record(
                    Record& record,
                    std::int32_t delta,
                    time::TimePoint change_time) -> void
                {
                    bool shield_broken = false;
                    auto effective_delta = static_cast<std::int64_t>(delta);

                    if (effective_delta < 0)
                    {
                        auto damage = -effective_delta;
                        const auto shield_current = std::max<std::int64_t>(
                            0,
                            record.resources.shield_current);
                        const auto absorbed = std::min(shield_current, damage);

                        if (absorbed > 0)
                        {
                            record.resources.shield_current = numeric::clamp_to_int32(
                                shield_current - absorbed);
                            damage -= absorbed;
                        }

                        if (record.resources.shield_current <= 0 &&
                            record.combat.status_effects.contains(status::Kind::shield))
                        {
                            shield_broken = record.combat.status_effects.remove(status::Kind::shield);
                            record.resources.shield_current = 0;
                        }

                        if (damage == 0)
                        {
                            if (shield_broken)
                            {
                                refresh_record(record);
                            }

                            return;
                        }

                        effective_delta = -damage;
                    }

                    record.resources.health_current = numeric::clamp_to_int32(
                        std::clamp<std::int64_t>(
                            static_cast<std::int64_t>(record.resources.health_current) +
                                effective_delta,
                            0,
                            record.stats.current_derived.max_hp));

                    if (record.resources.health_current <= 0)
                    {
                        const bool was_alive = record.lifecycle.alive;
                        record.lifecycle.alive = false;

                        if (was_alive)
                        {
                            record.lifecycle.death_at = change_time;
                            record.lifecycle.killer_entity_id.reset();
                            record.lifecycle.killing_source = damage::Source{};
                            record.lifecycle.killing_trace.reset();
                        }
                    }

                    if (shield_broken)
                    {
                        refresh_record(record);
                    }
                }

                // Recalculate derived stats and clamps from active status modifiers.
                auto refresh_record(Record& record) -> void
                {
                    const auto totals = record.combat.status_effects.aggregate();
                    refresh_record(record, totals);
                }

                auto refresh_record(Record& record, const status::Totals& totals) -> void
                {
                    const auto modifiers = amplify_positive_effects(totals.modifiers);

                    record.combat.guaranteed_critical_hit = modifiers.guaranteed_critical_hit;
                    record.combat.negative_status_immunity = modifiers.negative_status_immunity;

                    record.stats.current_primary = stat::add(record.stats.base_primary, modifiers.primary_delta);
                    stat::clamp_non_negative(record.stats.current_primary);

                    record.stats.current_derived = stat::add(
                        stat::derive(record.stats.current_primary),
                        modifiers.derived_delta);

                    const auto apply_percent = [](std::int32_t value, std::int32_t percent_delta) -> std::int32_t {
                        const auto effective_percent = std::max<std::int64_t>(
                            0,
                            100 + static_cast<std::int64_t>(percent_delta));
                        return numeric::scale_non_negative(
                            value,
                            static_cast<std::uint64_t>(effective_percent),
                            100);
                    };

                    record.stats.current_derived.max_hp = apply_percent(record.stats.current_derived.max_hp, modifiers.max_hp_percent_delta);
                    record.stats.current_derived.max_mana = apply_percent(record.stats.current_derived.max_mana, modifiers.max_mana_percent_delta);
                    record.stats.current_derived.attack = apply_percent(record.stats.current_derived.attack, modifiers.attack_percent_delta);
                    record.stats.current_derived.attack_speed = apply_percent(record.stats.current_derived.attack_speed, modifiers.attack_speed_percent_delta);
                    record.stats.current_derived.magic_attack = apply_percent(record.stats.current_derived.magic_attack, modifiers.magic_attack_percent_delta);
                    record.stats.current_derived.cast_speed = apply_percent(record.stats.current_derived.cast_speed, modifiers.cast_speed_percent_delta);
                    record.stats.current_derived.defense = apply_percent(record.stats.current_derived.defense, modifiers.defense_percent_delta);
                    record.stats.current_derived.magic_defense = apply_percent(record.stats.current_derived.magic_defense, modifiers.magic_defense_percent_delta);
                    record.stats.current_derived.crit_chance = apply_percent(record.stats.current_derived.crit_chance, modifiers.crit_chance_percent_delta);
                    record.stats.current_derived.crit_resist = apply_percent(record.stats.current_derived.crit_resist, modifiers.crit_resist_percent_delta);
                    record.stats.current_derived.magic_crit = apply_percent(record.stats.current_derived.magic_crit, modifiers.magic_crit_percent_delta);
                    record.stats.current_derived.magic_crit_res = apply_percent(record.stats.current_derived.magic_crit_res, modifiers.magic_crit_res_percent_delta);
                    record.stats.current_derived.move_speed = apply_percent(record.stats.current_derived.move_speed, modifiers.move_speed_percent_delta);

                    record.load.encumbrance = stat::derive_encumbrance(
                        record.load.carried_weight,
                        record.stats.current_derived.max_weight);

                    record.stats.current_derived.move_speed = apply_percent(
                        record.stats.current_derived.move_speed,
                        record.load.encumbrance.move_speed_percent_delta);

                    stat::clamp_non_negative(record.stats.current_derived);

                    if (totals.shield_absorb_percent_of_max_hp == 0 ||
                        !record.combat.status_effects.contains(status::Kind::shield))
                    {
                        record.resources.shield_current = 0;
                    }
                    else
                    {
                        const auto shield_cap = stat::scale(
                            record.stats.current_derived.max_hp,
                            static_cast<std::int32_t>(totals.shield_absorb_percent_of_max_hp),
                            100);

                        record.resources.shield_current = std::clamp(
                            record.resources.shield_current,
                            0,
                            shield_cap);
                    }

                    record.resources.health_current = std::clamp(
                        record.resources.health_current,
                        0,
                        record.stats.current_derived.max_hp);

                    record.resources.mana_current = std::clamp(
                        record.resources.mana_current,
                        0,
                        record.stats.current_derived.max_mana);

                    if (record.resources.health_current <= 0)
                    {
                        record.lifecycle.alive = false;
                    }
                }

                auto apply_status_side_effects(Record& record, const status::Instance& instance) -> void
                {
                    if (instance.kind == status::Kind::shield)
                    {
                        std::uint32_t shield_percent = 0;

                        for (const auto& layer : instance.shield_layers)
                        {
                            shield_percent = numeric::saturating_add(
                                shield_percent,
                                layer.absorb_percent_of_max_hp);
                        }

                        if (shield_percent > 0)
                        {
                            const auto shield_points = stat::scale(
                                record.stats.current_derived.max_hp,
                                static_cast<std::int32_t>(shield_percent),
                                100);

                            const auto shield_cap = stat::scale(
                                record.stats.current_derived.max_hp,
                                30,
                                100);

                            record.resources.shield_current = numeric::clamp_to_int32(
                                std::clamp<std::int64_t>(
                                    static_cast<std::int64_t>(record.resources.shield_current) +
                                        shield_points,
                                    0,
                                    shield_cap));
                        }
                    }

                    if (instance.kind == status::Kind::taunt &&
                        instance.source_entity_id.has_value() &&
                        instance.expires_at.has_value())
                    {
                        const auto duration = std::chrono::duration_cast<time::Milliseconds>(
                            instance.expires_at.value() - instance.applied_at);

                        if (duration.count() > 0)
                        {
                            record.combat.aggro.force_target(
                                instance.source_entity_id.value(),
                                duration,
                                instance.applied_at);
                        }
                    }

                    if (instance.kind == status::Kind::burn)
                    {
                        record.combat.status_effects.remove(status::Kind::regeneration);
                    }

                    if (instance.kind == status::Kind::daze)
                    {
                        record.combat.status_effects.remove(status::Kind::haste);
                    }

                    if (instance.kind == status::Kind::regeneration &&
                        record.combat.status_effects.contains(status::Kind::burn))
                    {
                        record.combat.status_effects.remove(status::Kind::regeneration);
                    }

                    if (instance.kind == status::Kind::haste &&
                        record.combat.status_effects.contains(status::Kind::daze))
                    {
                        record.combat.status_effects.remove(status::Kind::haste);
                    }
                }

                [[nodiscard]] auto amplify_positive_effects(status::Modifiers modifiers) const -> status::Modifiers
                {
                    if (modifiers.positive_effect_percent_delta <= 0)
                    {
                        return modifiers;
                    }

                    const auto positive_multiplier = static_cast<std::uint64_t>(
                        100 + static_cast<std::int64_t>(modifiers.positive_effect_percent_delta));

                    const auto amplify_positive = [positive_multiplier](std::int32_t& value) -> void {
                        if (value > 0)
                        {
                            value = numeric::scale_non_negative(value, positive_multiplier, 100);
                        }
                    };

                    amplify_positive(modifiers.max_hp_percent_delta);
                    amplify_positive(modifiers.max_mana_percent_delta);
                    amplify_positive(modifiers.attack_percent_delta);
                    amplify_positive(modifiers.attack_speed_percent_delta);
                    amplify_positive(modifiers.magic_attack_percent_delta);
                    amplify_positive(modifiers.cast_speed_percent_delta);
                    amplify_positive(modifiers.defense_percent_delta);
                    amplify_positive(modifiers.magic_defense_percent_delta);
                    amplify_positive(modifiers.crit_chance_percent_delta);
                    amplify_positive(modifiers.crit_resist_percent_delta);
                    amplify_positive(modifiers.magic_crit_percent_delta);
                    amplify_positive(modifiers.magic_crit_res_percent_delta);
                    amplify_positive(modifiers.move_speed_percent_delta);
                    amplify_positive(modifiers.healing_received_percent_delta);
                    amplify_positive(modifiers.natural_regen_percent_delta);
                    amplify_positive(modifiers.action_speed_percent_delta);
                    amplify_positive(modifiers.recovery_time_percent_delta);
                    amplify_positive(modifiers.incoming_damage_percent_delta);
                    amplify_positive(modifiers.incoming_physical_damage_percent_delta);
                    amplify_positive(modifiers.incoming_magical_damage_percent_delta);
                    amplify_positive(modifiers.incoming_build_up_percent_delta);
                    amplify_positive(modifiers.build_up_threshold_percent_delta);
                    amplify_positive(modifiers.build_up_decay_per_second_percent_delta);
                    amplify_positive(modifiers.break_damage_bonus_percent);

                    return modifiers;
                }

                auto link_to_zone(id::ZoneId zone_id, id::EntityId entity_id) -> void
                {
                    const auto entry = zone_index_.try_emplace(zone_id);
                    try
                    {
                        entry.first->second.insert(entity_id);
                    }
                    catch (...)
                    {
                        if (entry.second) zone_index_.erase(entry.first);
                        throw;
                    }
                }

                [[nodiscard]] auto has_zone_membership(id::ZoneId zone_id, id::EntityId entity_id) const -> bool
                {
                    const auto it = zone_index_.find(zone_id);
                    return it != zone_index_.end() && it->second.count(entity_id) != 0;
                }

                auto unlink_from_zone(id::ZoneId zone_id, id::EntityId entity_id) -> void
                {
                    auto zone_it = zone_index_.find(zone_id);
                    if (zone_it == zone_index_.end())
                    {
                        return;
                    }

                    zone_it->second.erase(entity_id);

                    if (zone_it->second.empty())
                    {
                        zone_index_.erase(zone_it);
                    }
                }
            };
        }
    }
}
