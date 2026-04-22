#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "id.hpp"
#include "stat.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace status
        {
            enum class Kind : std::uint8_t
            {
                poison = 0,
                burn,
                bleed,
                freeze,
                stun,
                silence,
                root,
                slow,
                haste,
                shield,
                regeneration,
                curse,
                bless,
                fear,
                vulnerable,
                resistant
            };

            enum class Category : std::uint8_t
            {
                buff = 0,
                debuff,
                control,
                damage_over_time,
                healing_over_time,
                shield,
                aura
            };

            enum class Delivery : std::uint8_t
            {
                instant = 0,
                build_up
            };

            enum class StackingMode : std::uint8_t
            {
                refresh_duration = 0,
                replace,
                stack
            };

            struct Definition
            {
                Kind kind{ Kind::poison };
                std::string_view name{};
                Category category{ Category::debuff };
                Delivery delivery{ Delivery::instant };
                StackingMode stacking_mode{ StackingMode::refresh_duration };
                std::uint32_t max_stacks{ 1 };
                std::optional<time::Milliseconds> duration{};
                std::optional<time::Milliseconds> tick_interval{};
                std::optional<time::Milliseconds> build_up_decay_delay{};
                std::uint32_t build_up_threshold{ 0 };
                std::uint32_t build_up_decay_per_second{ 0 };
                stat::Primary primary_delta{};
                stat::Derived derived_delta{};
                std::int32_t health_delta_per_tick{ 0 };
                std::int32_t mana_delta_per_tick{ 0 };
            };

            struct Instance
            {
                Kind kind{ Kind::poison };
                Delivery delivery{ Delivery::instant };
                StackingMode stacking_mode{ StackingMode::refresh_duration };
                std::uint32_t max_stacks{ 1 };
                std::uint32_t stacks{ 1 };
                time::TimePoint applied_at{};
                std::optional<time::TimePoint> expires_at{};
                std::optional<time::TimePoint> next_tick_at{};
                std::optional<id::EntityId> source_entity_id{};
                std::optional<id::SpeciesId> source_species_id{};
                std::optional<id::EntityTemplateId> source_template_id{};
                stat::Primary primary_delta{};
                stat::Derived derived_delta{};
                std::int32_t health_delta_per_tick{ 0 };
                std::int32_t mana_delta_per_tick{ 0 };
            };

            struct Meter
            {
                Kind kind{ Kind::poison };
                std::uint32_t current{ 0 };
                std::uint32_t threshold{ 0 };
                std::uint32_t decay_per_second{ 0 };
                std::optional<time::Milliseconds> decay_delay{};
                time::TimePoint applied_at{};
                time::TimePoint last_updated_at{};
                std::optional<id::EntityId> source_entity_id{};
                std::optional<id::SpeciesId> source_species_id{};
                std::optional<id::EntityTemplateId> source_template_id{};
            };

            struct Totals
            {
                stat::Primary primary_delta{};
                stat::Derived derived_delta{};
                std::int32_t health_delta_per_tick{ 0 };
                std::int32_t mana_delta_per_tick{ 0 };
            };

            [[nodiscard]] inline auto make_instance(
                const Definition& definition,
                time::TimePoint now,
                std::uint32_t stacks = 1,
                std::optional<id::EntityId> source_entity_id = {},
                std::optional<id::SpeciesId> source_species_id = {},
                std::optional<id::EntityTemplateId> source_template_id = {}) -> Instance
            {
                Instance instance{};
                instance.kind = definition.kind;
                instance.delivery = definition.delivery;
                instance.stacking_mode = definition.stacking_mode;
                instance.max_stacks = std::max<std::uint32_t>(1, definition.max_stacks);
                instance.stacks = std::clamp<std::uint32_t>(stacks, 1, instance.max_stacks);
                instance.applied_at = now;
                instance.source_entity_id = source_entity_id;
                instance.source_species_id = source_species_id;
                instance.source_template_id = source_template_id;
                instance.primary_delta = definition.primary_delta;
                instance.derived_delta = definition.derived_delta;
                instance.health_delta_per_tick = definition.health_delta_per_tick;
                instance.mana_delta_per_tick = definition.mana_delta_per_tick;

                if (definition.duration.has_value())
                {
                    instance.expires_at = now + definition.duration.value();
                }

                if (definition.tick_interval.has_value())
                {
                    instance.next_tick_at = now + definition.tick_interval.value();
                }

                return instance;
            }

            [[nodiscard]] inline auto make_meter(
                const Definition& definition,
                time::TimePoint now,
                std::optional<id::EntityId> source_entity_id = {},
                std::optional<id::SpeciesId> source_species_id = {},
                std::optional<id::EntityTemplateId> source_template_id = {}) -> Meter
            {
                Meter meter{};
                meter.kind = definition.kind;
                meter.current = 0;
                meter.threshold = std::max<std::uint32_t>(1, definition.build_up_threshold);
                meter.decay_per_second = definition.build_up_decay_per_second;
                meter.decay_delay = definition.build_up_decay_delay;
                meter.applied_at = now;
                meter.last_updated_at = now;
                meter.source_entity_id = source_entity_id;
                meter.source_species_id = source_species_id;
                meter.source_template_id = source_template_id;
                return meter;
            }

            class Catalog
            {
            public:
                auto insert(const Definition& definition) -> bool
                {
                    auto [it, inserted] = definitions_.emplace(definition.kind, definition);
                    (void)it;
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

                [[nodiscard]] auto instantiate(
                    Kind kind,
                    time::TimePoint now,
                    std::uint32_t stacks = 1,
                    std::optional<id::EntityId> source_entity_id = {},
                    std::optional<id::SpeciesId> source_species_id = {},
                    std::optional<id::EntityTemplateId> source_template_id = {}) const -> std::optional<Instance>
                {
                    auto definition_it = definitions_.find(kind);
                    if (definition_it == definitions_.end())
                    {
                        return std::nullopt;
                    }

                    return make_instance(definition_it->second, now, stacks, source_entity_id, source_species_id, source_template_id);
                }

                [[nodiscard]] auto build_meter(
                    Kind kind,
                    time::TimePoint now,
                    std::optional<id::EntityId> source_entity_id = {},
                    std::optional<id::SpeciesId> source_species_id = {},
                    std::optional<id::EntityTemplateId> source_template_id = {}) const -> std::optional<Meter>
                {
                    auto definition_it = definitions_.find(kind);
                    if (definition_it == definitions_.end())
                    {
                        return std::nullopt;
                    }

                    return make_meter(definition_it->second, now, source_entity_id, source_species_id, source_template_id);
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

            class Table
            {
            public:
                auto apply(const Instance& instance) -> void
                {
                    auto instance_it = find_instance(instance.kind);
                    if (instance_it != instances_.end())
                    {
                        auto& current = *instance_it;

                        if (instance.stacking_mode == StackingMode::replace)
                        {
                            current = instance;
                            return;
                        }

                        if (instance.stacking_mode == StackingMode::stack)
                        {
                            current.stacks = std::clamp(current.stacks + instance.stacks, 1u, instance.max_stacks);
                            current.applied_at = instance.applied_at;
                            current.expires_at = instance.expires_at;
                            current.next_tick_at = instance.next_tick_at;
                            current.source_entity_id = instance.source_entity_id;
                            current.source_species_id = instance.source_species_id;
                            current.source_template_id = instance.source_template_id;
                            current.primary_delta = instance.primary_delta;
                            current.derived_delta = instance.derived_delta;
                            current.health_delta_per_tick = instance.health_delta_per_tick;
                            current.mana_delta_per_tick = instance.mana_delta_per_tick;
                            return;
                        }

                        current.stacks = std::max(current.stacks, instance.stacks);
                        current.applied_at = instance.applied_at;
                        current.expires_at = instance.expires_at;
                        current.next_tick_at = instance.next_tick_at;
                        current.source_entity_id = instance.source_entity_id;
                        current.source_species_id = instance.source_species_id;
                        current.source_template_id = instance.source_template_id;
                        current.primary_delta = instance.primary_delta;
                        current.derived_delta = instance.derived_delta;
                        current.health_delta_per_tick = instance.health_delta_per_tick;
                        current.mana_delta_per_tick = instance.mana_delta_per_tick;
                        return;
                    }

                    instances_.push_back(instance);
                }

                [[nodiscard]] auto accumulate(
                    const Definition& definition,
                    std::uint32_t amount,
                    time::TimePoint now,
                    std::optional<id::EntityId> source_entity_id = {},
                    std::optional<id::SpeciesId> source_species_id = {},
                    std::optional<id::EntityTemplateId> source_template_id = {}) -> std::optional<Instance>
                {
                    if (definition.delivery == Delivery::instant)
                    {
                        auto instance = make_instance(definition, now, 1, source_entity_id, source_species_id, source_template_id);
                        apply(instance);
                        return instance;
                    }

                    auto meter = ensure_meter(definition, now, source_entity_id, source_species_id, source_template_id);
                    advance_meter(meter, now);

                    meter.current = std::min(meter.threshold, meter.current + amount);
                    meter.last_updated_at = now;

                    if (meter.current < meter.threshold)
                    {
                        return std::nullopt;
                    }

                    auto proc_instance = make_instance(definition, now, 1, source_entity_id, source_species_id, source_template_id);
                    remove_pending(definition.kind);
                    apply(proc_instance);
                    return proc_instance;
                }

                auto remove(Kind kind) -> bool
                {
                    auto instance_it = find_instance(kind);
                    if (instance_it == instances_.end())
                    {
                        return false;
                    }

                    instances_.erase(instance_it);
                    return true;
                }

                auto remove_pending(Kind kind) -> bool
                {
                    auto meter_it = find_meter(kind);
                    if (meter_it == meters_.end())
                    {
                        return false;
                    }

                    meters_.erase(meter_it);
                    return true;
                }

                [[nodiscard]] auto find(Kind kind) -> Instance*
                {
                    auto instance_it = find_instance(kind);
                    if (instance_it == instances_.end())
                    {
                        return nullptr;
                    }

                    return &(*instance_it);
                }

                [[nodiscard]] auto find(Kind kind) const -> const Instance*
                {
                    auto instance_it = find_instance(kind);
                    if (instance_it == instances_.end())
                    {
                        return nullptr;
                    }

                    return &(*instance_it);
                }

                [[nodiscard]] auto find_pending(Kind kind) -> Meter*
                {
                    auto meter_it = find_meter(kind);
                    if (meter_it == meters_.end())
                    {
                        return nullptr;
                    }

                    return &(*meter_it);
                }

                [[nodiscard]] auto find_pending(Kind kind) const -> const Meter*
                {
                    auto meter_it = find_meter(kind);
                    if (meter_it == meters_.end())
                    {
                        return nullptr;
                    }

                    return &(*meter_it);
                }

                [[nodiscard]] auto contains(Kind kind) const -> bool
                {
                    return find_instance(kind) != instances_.end();
                }

                auto remove_expired(time::TimePoint now) -> std::vector<Instance>
                {
                    std::vector<Instance> removed_instances;
                    auto instance_it = instances_.begin();
                    while (instance_it != instances_.end())
                    {
                        if (instance_it->expires_at.has_value() && instance_it->expires_at.value() <= now)
                        {
                            removed_instances.push_back(*instance_it);
                            instance_it = instances_.erase(instance_it);
                        }
                        else
                        {
                            ++instance_it;
                        }
                    }

                    return removed_instances;
                }

                auto decay_build_up(time::TimePoint now) -> bool
                {
                    bool changed = false;
                    auto meter_it = meters_.begin();
                    while (meter_it != meters_.end())
                    {
                        auto& meter = *meter_it;
                        if (advance_meter(meter, now))
                        {
                            changed = true;
                        }

                        if (meter.current == 0)
                        {
                            meter_it = meters_.erase(meter_it);
                            changed = true;
                        }
                        else
                        {
                            ++meter_it;
                        }
                    }

                    return changed;
                }

                auto sweep(time::TimePoint now) -> bool
                {
                    const auto removed_instances = remove_expired(now);
                    const auto decay_changed = decay_build_up(now);
                    return !removed_instances.empty() || decay_changed;
                }

                [[nodiscard]] auto aggregate() const -> Totals
                {
                    Totals totals{};
                    for (const auto& instance : instances_)
                    {
                        totals.primary_delta = stat::add(totals.primary_delta, instance.primary_delta);
                        totals.derived_delta = stat::add(totals.derived_delta, instance.derived_delta);
                        totals.health_delta_per_tick += instance.health_delta_per_tick;
                        totals.mana_delta_per_tick += instance.mana_delta_per_tick;
                    }

                    return totals;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return instances_.size();
                }

                [[nodiscard]] auto pending_size() const noexcept -> std::size_t
                {
                    return meters_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return instances_.empty() && meters_.empty();
                }

            private:
                std::vector<Instance> instances_;
                std::vector<Meter> meters_;

                [[nodiscard]] auto find_instance(Kind kind) -> std::vector<Instance>::iterator
                {
                    return std::find_if(instances_.begin(), instances_.end(), [kind](const Instance& instance) {
                        return instance.kind == kind;
                    });
                }

                [[nodiscard]] auto find_instance(Kind kind) const -> std::vector<Instance>::const_iterator
                {
                    return std::find_if(instances_.begin(), instances_.end(), [kind](const Instance& instance) {
                        return instance.kind == kind;
                    });
                }

                [[nodiscard]] auto find_meter(Kind kind) -> std::vector<Meter>::iterator
                {
                    return std::find_if(meters_.begin(), meters_.end(), [kind](const Meter& meter) {
                        return meter.kind == kind;
                    });
                }

                [[nodiscard]] auto find_meter(Kind kind) const -> std::vector<Meter>::const_iterator
                {
                    return std::find_if(meters_.begin(), meters_.end(), [kind](const Meter& meter) {
                        return meter.kind == kind;
                    });
                }

                auto ensure_meter(
                    const Definition& definition,
                    time::TimePoint now,
                    std::optional<id::EntityId> source_entity_id,
                    std::optional<id::SpeciesId> source_species_id,
                    std::optional<id::EntityTemplateId> source_template_id) -> Meter&
                {
                    auto meter_it = find_meter(definition.kind);
                    if (meter_it == meters_.end())
                    {
                        meters_.push_back(make_meter(definition, now, source_entity_id, source_species_id, source_template_id));
                        return meters_.back();
                    }

                    auto& meter = *meter_it;
                    meter.threshold = std::max<std::uint32_t>(1, definition.build_up_threshold);
                    meter.decay_per_second = definition.build_up_decay_per_second;
                    meter.decay_delay = definition.build_up_decay_delay;
                    meter.source_entity_id = source_entity_id;
                    meter.source_species_id = source_species_id;
                    meter.source_template_id = source_template_id;
                    return meter;
                }

                auto advance_meter(Meter& meter, time::TimePoint now) -> bool
                {
                    if (!meter.decay_delay.has_value() || meter.decay_per_second == 0)
                    {
                        meter.last_updated_at = now;
                        return false;
                    }

                    const auto elapsed = std::chrono::duration_cast<time::Milliseconds>(now - meter.last_updated_at);
                    if (elapsed <= meter.decay_delay.value())
                    {
                        return false;
                    }

                    const auto decay_window = elapsed - meter.decay_delay.value();
                    const auto decay_amount = static_cast<std::uint32_t>((decay_window.count() * meter.decay_per_second) / 1000);
                    if (decay_amount == 0)
                    {
                        return false;
                    }

                    const auto previous_value = meter.current;
                    meter.current = (decay_amount >= meter.current) ? 0 : (meter.current - decay_amount);
                    meter.last_updated_at = now;
                    return meter.current != previous_value;
                }
            };
        }
    }
}
