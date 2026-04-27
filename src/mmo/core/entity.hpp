#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "id.hpp"
#include "action.hpp"
#include "combat.hpp"
#include "stat.hpp"
#include "status.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace entity
        {
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
            };

            struct Identity
            {
                id::EntityId entity_id{ id::invalid_entity_id };
                id::EntityTemplateId template_id{ id::invalid_entity_template_id };
                id::SpeciesId species_id{ id::invalid_species_id };
                Type type{ Type::unknown };
            };

            struct Placement
            {
                id::ZoneId zone_id{ id::invalid_zone_id };
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

            struct Combat
            {
                status::Table status_effects{};
                action::State action_state{};
                std::optional<time::TimePoint> action_ready_at{};
                combat::AggroState aggro{};
                bool guaranteed_critical_hit{ false };
                bool negative_status_immunity{ false };
            };

            struct Lifecycle
            {
                time::TimePoint spawned_at{};
                std::optional<time::TimePoint> last_damage_at{};
                bool alive{ true };
            };

            struct Contract
            {
                Identity identity{};
                Placement placement{};
                Stats stats{};
                Resources resources{};
                Combat combat{};
                Lifecycle lifecycle{};
            };

            using Record = Contract;

            class Catalog
            {
            public:
                auto insert(const Blueprint& blueprint) -> bool
                {
                    auto insert_result = blueprints_.emplace(blueprint.template_id, blueprint);
                    auto& it = insert_result.first;
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
                std::unordered_map<id::EntityTemplateId, Blueprint> blueprints_;
                std::unordered_map<id::SpeciesId, std::vector<id::EntityTemplateId>> species_index_;

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
            public:
                auto spawn(id::EntityId entity_id, const Blueprint& blueprint, id::ZoneId zone_id, time::TimePoint now) -> bool
                {
                    Record record{};
                    record.identity.entity_id = entity_id;
                    record.identity.template_id = blueprint.template_id;
                    record.identity.species_id = blueprint.species_id;
                    record.identity.type = blueprint.type;
                    record.placement.zone_id = zone_id;
                    record.stats.base_primary = blueprint.base_primary;
                    record.stats.current_primary = blueprint.base_primary;
                    record.stats.current_derived = stat::derive(record.stats.current_primary);
                    record.resources.health_current = record.stats.current_derived.max_hp;
                    record.resources.mana_current = record.stats.current_derived.max_mana;
                    record.lifecycle.spawned_at = now;
                    record.lifecycle.alive = true;
                    return insert(record);
                }

                auto insert(const Record& record) -> bool
                {
                    auto insert_result = records_.emplace(record.identity.entity_id, record);
                    auto& it = insert_result.first;
                    const bool inserted = insert_result.second;
                    if (!inserted)
                    {
                        return false;
                    }

                    refresh_record(it->second);
                    zone_index_[record.placement.zone_id].insert(record.identity.entity_id);
                    return true;
                }

                auto erase(id::EntityId entity_id) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    unlink_from_zone(record_it->second.placement.zone_id, entity_id);
                    records_.erase(record_it);
                    return true;
                }

                [[nodiscard]] auto find(id::EntityId entity_id) -> Record*
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return nullptr;
                    }

                    return &record_it->second;
                }

                [[nodiscard]] auto find(id::EntityId entity_id) const -> const Record*
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return nullptr;
                    }

                    return &record_it->second;
                }

                auto move_to_zone(id::EntityId entity_id, id::ZoneId zone_id) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    if (record_it->second.placement.zone_id == zone_id)
                    {
                        zone_index_[zone_id].insert(entity_id);
                        return true;
                    }

                    unlink_from_zone(record_it->second.placement.zone_id, entity_id);
                    record_it->second.placement.zone_id = zone_id;
                    zone_index_[zone_id].insert(entity_id);
                    return true;
                }

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

                auto set_action_state(id::EntityId entity_id, const action::State& action_state, time::TimePoint ready_at) -> bool
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

                    if (definition.delivery == status::Delivery::build_up && definition.kind != status::Kind::curse && record_it->second.combat.status_effects.contains(status::Kind::shield))
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

                auto sweep_statuses(id::EntityId entity_id, time::TimePoint now) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    const auto active_totals = record_it->second.combat.status_effects.aggregate();
                    const auto active_modifiers = amplify_positive_effects(active_totals.modifiers);
                    const auto changed = record_it->second.combat.status_effects.sweep(now, active_modifiers.build_up_decay_per_second_percent_delta);
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
                        const auto taunt_expired = std::any_of(removed_statuses.begin(), removed_statuses.end(), [](const status::Instance& removed_status) {
                            return removed_status.kind == status::Kind::taunt;
                        });

                        const auto shield_expired = std::any_of(removed_statuses.begin(), removed_statuses.end(), [](const status::Instance& removed_status) {
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

                auto adjust_health(id::EntityId entity_id, std::int32_t delta) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    bool shield_broken = false;

                    if (delta < 0)
                    {
                        auto damage = -delta;
                        const auto absorbed = std::min(record_it->second.resources.shield_current, damage);
                        if (absorbed > 0)
                        {
                            record_it->second.resources.shield_current -= absorbed;
                            damage -= absorbed;
                        }

                        if (record_it->second.resources.shield_current <= 0 && record_it->second.combat.status_effects.contains(status::Kind::shield))
                        {
                            shield_broken = record_it->second.combat.status_effects.remove(status::Kind::shield);
                            record_it->second.resources.shield_current = 0;
                        }

                        if (damage == 0)
                        {
                            if (shield_broken)
                            {
                                refresh_record(record_it->second);
                            }

                            return true;
                        }

                        delta = -damage;
                    }

                    record_it->second.resources.health_current = std::clamp(record_it->second.resources.health_current + delta, 0, record_it->second.stats.current_derived.max_hp);
                    if (record_it->second.resources.health_current <= 0)
                    {
                        record_it->second.lifecycle.alive = false;
                    }

                    if (shield_broken)
                    {
                        refresh_record(record_it->second);
                    }

                    return true;
                }

                auto adjust_mana(id::EntityId entity_id, std::int32_t delta) -> bool
                {
                    auto record_it = records_.find(entity_id);
                    if (record_it == records_.end())
                    {
                        return false;
                    }

                    record_it->second.resources.mana_current = std::clamp(record_it->second.resources.mana_current + delta, 0, record_it->second.stats.current_derived.max_mana);
                    return true;
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

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return records_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return records_.empty();
                }

            private:
                std::unordered_map<id::EntityId, Record> records_;
                std::unordered_map<id::ZoneId, std::unordered_set<id::EntityId>> zone_index_;

                auto refresh_record(Record& record) -> void
                {
                    const auto totals = record.combat.status_effects.aggregate();
                    const auto modifiers = amplify_positive_effects(totals.modifiers);

                    record.combat.guaranteed_critical_hit = modifiers.guaranteed_critical_hit;
                    record.combat.negative_status_immunity = modifiers.negative_status_immunity;

                    record.stats.current_primary = stat::add(record.stats.base_primary, modifiers.primary_delta);
                    stat::clamp_non_negative(record.stats.current_primary);

                    record.stats.current_derived = stat::add(stat::derive(record.stats.current_primary), modifiers.derived_delta);

                    const auto apply_percent = [](std::int32_t value, std::int32_t percent_delta) -> std::int32_t {
                        return stat::scale(value, std::max<std::int32_t>(0, 100 + percent_delta), 100);
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
                    stat::clamp_non_negative(record.stats.current_derived);

                    if (totals.shield_absorb_percent_of_max_hp == 0 || !record.combat.status_effects.contains(status::Kind::shield))
                    {
                        record.resources.shield_current = 0;
                    }
                    else
                    {
                        const auto shield_cap = stat::scale(record.stats.current_derived.max_hp, static_cast<std::int32_t>(totals.shield_absorb_percent_of_max_hp), 100);
                        record.resources.shield_current = std::clamp(record.resources.shield_current, 0, shield_cap);
                    }

                    record.resources.health_current = std::clamp(record.resources.health_current, 0, record.stats.current_derived.max_hp);
                    record.resources.mana_current = std::clamp(record.resources.mana_current, 0, record.stats.current_derived.max_mana);

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
                            shield_percent += layer.absorb_percent_of_max_hp;
                        }

                        if (shield_percent > 0)
                        {
                            const auto shield_points = stat::scale(record.stats.current_derived.max_hp, static_cast<std::int32_t>(shield_percent), 100);
                            const auto shield_cap = stat::scale(record.stats.current_derived.max_hp, 30, 100);
                            record.resources.shield_current = std::clamp(record.resources.shield_current + shield_points, 0, shield_cap);
                        }
                    }

                    if (instance.kind == status::Kind::taunt && instance.source_entity_id.has_value() && instance.expires_at.has_value())
                    {
                        const auto duration = std::chrono::duration_cast<time::Milliseconds>(instance.expires_at.value() - instance.applied_at);
                        if (duration.count() > 0)
                        {
                            record.combat.aggro.force_target(instance.source_entity_id.value(), duration, instance.applied_at);
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

                    if (instance.kind == status::Kind::regeneration && record.combat.status_effects.contains(status::Kind::burn))
                    {
                        record.combat.status_effects.remove(status::Kind::regeneration);
                    }

                    if (instance.kind == status::Kind::haste && record.combat.status_effects.contains(status::Kind::daze))
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

                    const auto positive_multiplier = 100 + modifiers.positive_effect_percent_delta;
                    const auto amplify_positive = [positive_multiplier](std::int32_t& value) -> void {
                        if (value > 0)
                        {
                            value = static_cast<std::int32_t>((static_cast<std::int64_t>(value) * positive_multiplier) / 100);
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
