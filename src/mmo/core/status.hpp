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
                daze,
                silence,
                root,
                haste,
                shield,
                regeneration,
                curse,
                bless,
                fear,
                taunt,
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

            enum class Outcome : std::uint8_t
            {
                applied = 0,
                built_up,
                activated,
                refreshed,
                stacked,
                ticked,
                broken,
                expired,
                cleansed
            };

            struct Modifiers
            {
                stat::Primary primary_delta{};
                stat::Derived derived_delta{};
                std::int32_t health_delta_per_tick{ 0 };
                std::int32_t mana_delta_per_tick{ 0 };
                std::int32_t max_hp_percent_delta{ 0 };
                std::int32_t max_mana_percent_delta{ 0 };
                std::int32_t attack_percent_delta{ 0 };
                std::int32_t attack_speed_percent_delta{ 0 };
                std::int32_t magic_attack_percent_delta{ 0 };
                std::int32_t cast_speed_percent_delta{ 0 };
                std::int32_t defense_percent_delta{ 0 };
                std::int32_t magic_defense_percent_delta{ 0 };
                std::int32_t accuracy_percent_delta{ 0 };
                std::int32_t crit_chance_percent_delta{ 0 };
                std::int32_t evasion_percent_delta{ 0 };
                std::int32_t crit_resist_percent_delta{ 0 };
                std::int32_t magic_crit_percent_delta{ 0 };
                std::int32_t magic_crit_res_percent_delta{ 0 };
                std::int32_t move_speed_percent_delta{ 0 };
                std::int32_t healing_received_percent_delta{ 0 };
                std::int32_t natural_regen_percent_delta{ 0 };
                std::int32_t action_speed_percent_delta{ 0 };
                std::int32_t recovery_time_percent_delta{ 0 };
                std::int32_t incoming_damage_percent_delta{ 0 };
                std::int32_t incoming_physical_damage_percent_delta{ 0 };
                std::int32_t incoming_magical_damage_percent_delta{ 0 };
                std::int32_t incoming_build_up_percent_delta{ 0 };
                std::int32_t build_up_threshold_percent_delta{ 0 };
                std::int32_t build_up_decay_per_second_percent_delta{ 0 };
                std::int32_t positive_effect_percent_delta{ 0 };
                bool suppress_movement{ false };
                bool suppress_skill_usage{ false };
                bool suppress_action_execution{ false };
                bool break_on_physical_hit{ false };
                bool break_on_magical_hit{ false };
                bool guaranteed_critical_hit{ false };
                bool negative_status_immunity{ false };
                std::int32_t break_damage_bonus_percent{ 0 };
            };

            struct ShieldLayer
            {
                std::uint32_t absorb_percent_of_max_hp{ 0 };
                bool has_bonus{ false };
                Modifiers bonus_modifiers{};
            };

            struct StackStage
            {
                std::uint32_t minimum_stacks{ 1 };
                Modifiers modifiers{};
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
                std::uint32_t build_up_cap_percent{ 100 };
                bool keep_build_up_after_activate{ false };
                std::uint32_t shield_absorb_percent_of_max_hp{ 0 };
                bool shield_bonus_enabled{ false };
                Modifiers modifiers{};
                std::vector<StackStage> stack_stages{};
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
                Modifiers modifiers{};
                std::vector<ShieldLayer> shield_layers{};
                std::vector<StackStage> stack_stages{};
            };

            struct Meter
            {
                Kind kind{ Kind::poison };
                std::uint32_t current{ 0 };
                std::uint32_t threshold{ 0 };
                std::uint32_t capacity{ 0 };
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
                Modifiers modifiers{};
                std::uint32_t shield_absorb_percent_of_max_hp{ 0 };
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
                if (definition.kind == Kind::shield)
                {
                    instance.modifiers = Modifiers{};
                    if (definition.shield_absorb_percent_of_max_hp > 0)
                    {
                        instance.shield_layers.push_back(ShieldLayer{
                            definition.shield_absorb_percent_of_max_hp,
                            definition.shield_bonus_enabled,
                            definition.modifiers
                        });
                    }
                }
                else
                {
                    instance.modifiers = definition.modifiers;
                }
                instance.stack_stages = definition.stack_stages;

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
                meter.capacity = std::max<std::uint32_t>(meter.threshold, (meter.threshold * std::max<std::uint32_t>(1, definition.build_up_cap_percent)) / 100);
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

            [[nodiscard]] inline auto make_taunt_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::taunt;
                definition.name = "Taunt";
                definition.category = Category::control;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 1800 };
                definition.tick_interval.reset();
                definition.build_up_decay_delay = time::Milliseconds{ 1200 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 18;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;
                return definition;
            }

            [[nodiscard]] inline auto is_negative_kind(Kind kind) -> bool
            {
                switch (kind)
                {
                    case Kind::poison:
                    case Kind::burn:
                    case Kind::bleed:
                    case Kind::freeze:
                    case Kind::daze:
                    case Kind::silence:
                    case Kind::root:
                    case Kind::curse:
                    case Kind::fear:
                    case Kind::taunt:
                    case Kind::vulnerable:
                        return true;
                    default:
                        return false;
                }
            }

            [[nodiscard]] inline auto make_shield_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::shield;
                definition.name = "Shield";
                definition.category = Category::shield;
                definition.delivery = Delivery::instant;
                definition.stacking_mode = StackingMode::stack;
                definition.max_stacks = 30;
                definition.shield_absorb_percent_of_max_hp = 10;
                definition.shield_bonus_enabled = false;
                return definition;
            }

            [[nodiscard]] inline auto make_haste_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::haste;
                definition.name = "Haste";
                definition.category = Category::buff;
                definition.delivery = Delivery::instant;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 6000 };
                definition.modifiers.move_speed_percent_delta = 20;
                definition.modifiers.action_speed_percent_delta = 20;
                return definition;
            }

            [[nodiscard]] inline auto make_regeneration_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::regeneration;
                definition.name = "Regeneration";
                definition.category = Category::buff;
                definition.delivery = Delivery::instant;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 12000 };
                definition.modifiers.healing_received_percent_delta = 25;
                definition.modifiers.natural_regen_percent_delta = 40;
                return definition;
            }

            [[nodiscard]] inline auto make_bless_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::bless;
                definition.name = "Bless";
                definition.category = Category::buff;
                definition.delivery = Delivery::instant;
                definition.stacking_mode = StackingMode::stack;
                definition.max_stacks = 5;
                definition.duration = time::Milliseconds{ 5000 };
                definition.modifiers.break_damage_bonus_percent = 0;

                StackStage stage_one{};
                stage_one.minimum_stacks = 1;
                stage_one.modifiers.attack_percent_delta = 15;

                StackStage stage_two{};
                stage_two.minimum_stacks = 2;
                stage_two.modifiers.defense_percent_delta = 15;
                stage_two.modifiers.magic_defense_percent_delta = 15;

                StackStage stage_three{};
                stage_three.minimum_stacks = 3;
                stage_three.modifiers.build_up_threshold_percent_delta = 100;

                StackStage stage_four{};
                stage_four.minimum_stacks = 4;
                stage_four.modifiers.guaranteed_critical_hit = true;

                StackStage stage_five{};
                stage_five.minimum_stacks = 5;
                stage_five.modifiers.negative_status_immunity = true;
                stage_five.modifiers.positive_effect_percent_delta = 100;

                definition.stack_stages.push_back(stage_one);
                definition.stack_stages.push_back(stage_two);
                definition.stack_stages.push_back(stage_three);
                definition.stack_stages.push_back(stage_four);
                definition.stack_stages.push_back(stage_five);
                return definition;
            }

            [[nodiscard]] inline auto make_resistant_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::resistant;
                definition.name = "Resistant";
                definition.category = Category::buff;
                definition.delivery = Delivery::instant;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 7000 };
                definition.modifiers.build_up_threshold_percent_delta = 25;
                definition.modifiers.build_up_decay_per_second_percent_delta = 30;
                return definition;
            }

            [[nodiscard]] inline auto make_positive_buffs_catalog() -> Catalog
            {
                Catalog catalog{};
                catalog.insert(make_shield_definition());
                catalog.insert(make_haste_definition());
                catalog.insert(make_regeneration_definition());
                catalog.insert(make_bless_definition());
                catalog.insert(make_resistant_definition());
                return catalog;
            }

            [[nodiscard]] inline auto add_modifiers(const Modifiers& lhs, const Modifiers& rhs) -> Modifiers
            {
                Modifiers result = lhs;
                result.primary_delta = stat::add(result.primary_delta, rhs.primary_delta);
                result.derived_delta = stat::add(result.derived_delta, rhs.derived_delta);
                result.health_delta_per_tick += rhs.health_delta_per_tick;
                result.mana_delta_per_tick += rhs.mana_delta_per_tick;
                result.max_hp_percent_delta += rhs.max_hp_percent_delta;
                result.max_mana_percent_delta += rhs.max_mana_percent_delta;
                result.attack_percent_delta += rhs.attack_percent_delta;
                result.attack_speed_percent_delta += rhs.attack_speed_percent_delta;
                result.magic_attack_percent_delta += rhs.magic_attack_percent_delta;
                result.cast_speed_percent_delta += rhs.cast_speed_percent_delta;
                result.defense_percent_delta += rhs.defense_percent_delta;
                result.magic_defense_percent_delta += rhs.magic_defense_percent_delta;
                result.accuracy_percent_delta += rhs.accuracy_percent_delta;
                result.crit_chance_percent_delta += rhs.crit_chance_percent_delta;
                result.evasion_percent_delta += rhs.evasion_percent_delta;
                result.crit_resist_percent_delta += rhs.crit_resist_percent_delta;
                result.magic_crit_percent_delta += rhs.magic_crit_percent_delta;
                result.magic_crit_res_percent_delta += rhs.magic_crit_res_percent_delta;
                result.move_speed_percent_delta += rhs.move_speed_percent_delta;
                result.healing_received_percent_delta += rhs.healing_received_percent_delta;
                result.natural_regen_percent_delta += rhs.natural_regen_percent_delta;
                result.action_speed_percent_delta += rhs.action_speed_percent_delta;
                result.recovery_time_percent_delta += rhs.recovery_time_percent_delta;
                result.incoming_damage_percent_delta += rhs.incoming_damage_percent_delta;
                result.incoming_physical_damage_percent_delta += rhs.incoming_physical_damage_percent_delta;
                result.incoming_magical_damage_percent_delta += rhs.incoming_magical_damage_percent_delta;
                result.incoming_build_up_percent_delta += rhs.incoming_build_up_percent_delta;
                result.build_up_threshold_percent_delta += rhs.build_up_threshold_percent_delta;
                result.build_up_decay_per_second_percent_delta += rhs.build_up_decay_per_second_percent_delta;
                result.positive_effect_percent_delta += rhs.positive_effect_percent_delta;
                result.suppress_movement = result.suppress_movement || rhs.suppress_movement;
                result.suppress_skill_usage = result.suppress_skill_usage || rhs.suppress_skill_usage;
                result.suppress_action_execution = result.suppress_action_execution || rhs.suppress_action_execution;
                result.break_on_physical_hit = result.break_on_physical_hit || rhs.break_on_physical_hit;
                result.break_on_magical_hit = result.break_on_magical_hit || rhs.break_on_magical_hit;
                result.guaranteed_critical_hit = result.guaranteed_critical_hit || rhs.guaranteed_critical_hit;
                result.negative_status_immunity = result.negative_status_immunity || rhs.negative_status_immunity;
                result.break_damage_bonus_percent += rhs.break_damage_bonus_percent;
                return result;
            }

            [[nodiscard]] inline auto make_poison_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::poison;
                definition.name = "Poison";
                definition.category = Category::damage_over_time;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 8000 };
                definition.tick_interval = time::Milliseconds{ 1000 };
                definition.build_up_decay_delay = time::Milliseconds{ 1000 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 12;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;
                definition.modifiers.health_delta_per_tick = -8;
                definition.modifiers.defense_percent_delta = -30;
                return definition;
            }

            [[nodiscard]] inline auto make_burn_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::burn;
                definition.name = "Burn";
                definition.category = Category::damage_over_time;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 12000 };
                definition.tick_interval = time::Milliseconds{ 1000 };
                definition.build_up_decay_delay = time::Milliseconds{ 0 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 10;
                definition.build_up_cap_percent = 125;
                definition.keep_build_up_after_activate = true;
                definition.modifiers.health_delta_per_tick = -14;
                definition.modifiers.healing_received_percent_delta = -50;
                definition.modifiers.natural_regen_percent_delta = -50;
                return definition;
            }

            [[nodiscard]] inline auto make_bleed_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::bleed;
                definition.name = "Bleed";
                definition.category = Category::damage_over_time;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::stack;
                definition.max_stacks = 5;
                definition.duration = time::Milliseconds{ 6000 };
                definition.tick_interval = time::Milliseconds{ 750 };
                definition.build_up_decay_delay = time::Milliseconds{ 1000 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 14;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;
                definition.modifiers.health_delta_per_tick = -2;

                StackStage stage_two{};
                stage_two.minimum_stacks = 2;
                stage_two.modifiers.health_delta_per_tick = -1;

                StackStage stage_three{};
                stage_three.minimum_stacks = 3;
                stage_three.modifiers.health_delta_per_tick = -2;

                StackStage stage_four{};
                stage_four.minimum_stacks = 4;
                stage_four.modifiers.health_delta_per_tick = -3;

                StackStage stage_five{};
                stage_five.minimum_stacks = 5;
                stage_five.modifiers.health_delta_per_tick = -5;

                definition.stack_stages.push_back(stage_two);
                definition.stack_stages.push_back(stage_three);
                definition.stack_stages.push_back(stage_four);
                definition.stack_stages.push_back(stage_five);
                return definition;
            }

            [[nodiscard]] inline auto make_freeze_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::freeze;
                definition.name = "Freeze";
                definition.category = Category::control;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 3000 };
                definition.tick_interval.reset();
                definition.build_up_decay_delay = time::Milliseconds{ 1000 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 8;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;
                definition.modifiers.move_speed_percent_delta = -100;
                definition.modifiers.suppress_movement = true;
                definition.modifiers.suppress_action_execution = true;
                definition.modifiers.break_on_physical_hit = true;
                definition.modifiers.break_on_magical_hit = true;
                definition.modifiers.break_damage_bonus_percent = 50;
                return definition;
            }

            [[nodiscard]] inline auto make_daze_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::daze;
                definition.name = "Daze";
                definition.category = Category::control;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 6000 };
                definition.tick_interval.reset();
                definition.build_up_decay_delay = time::Milliseconds{ 1000 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 10;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;
                definition.modifiers.attack_speed_percent_delta = -25;
                definition.modifiers.cast_speed_percent_delta = -20;
                definition.modifiers.action_speed_percent_delta = -20;
                definition.modifiers.recovery_time_percent_delta = 25;
                return definition;
            }

            [[nodiscard]] inline auto make_silence_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::silence;
                definition.name = "Silence";
                definition.category = Category::control;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 8000 };
                definition.tick_interval.reset();
                definition.build_up_decay_delay = time::Milliseconds{ 1000 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 8;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;
                definition.modifiers.suppress_skill_usage = true;
                return definition;
            }

            [[nodiscard]] inline auto make_root_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::root;
                definition.name = "Root";
                definition.category = Category::control;
                definition.delivery = Delivery::build_up;
                definition.stacking_mode = StackingMode::refresh_duration;
                definition.max_stacks = 1;
                definition.duration = time::Milliseconds{ 5000 };
                definition.tick_interval.reset();
                definition.build_up_decay_delay = time::Milliseconds{ 1000 };
                definition.build_up_threshold = 100;
                definition.build_up_decay_per_second = 8;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;
                definition.modifiers.suppress_movement = true;
                definition.modifiers.move_speed_percent_delta = -100;
                definition.modifiers.evasion_percent_delta = -30;
                definition.modifiers.build_up_threshold_percent_delta = 20;
                return definition;
            }

            [[nodiscard]] inline auto make_curse_definition() -> Definition
            {
                Definition definition{};
                definition.kind = Kind::curse;
                definition.name = "Curse";
                definition.category = Category::debuff;
                definition.delivery = Delivery::instant;
                definition.stacking_mode = StackingMode::stack;
                definition.max_stacks = 5;
                definition.duration.reset();
                definition.tick_interval.reset();
                definition.build_up_decay_delay.reset();
                definition.build_up_threshold = 0;
                definition.build_up_decay_per_second = 0;
                definition.build_up_cap_percent = 100;
                definition.keep_build_up_after_activate = false;

                StackStage stage_one{};
                stage_one.minimum_stacks = 1;
                stage_one.modifiers.attack_percent_delta = -15;

                StackStage stage_two{};
                stage_two.minimum_stacks = 2;
                stage_two.modifiers.defense_percent_delta = -15;

                StackStage stage_three{};
                stage_three.minimum_stacks = 3;
                stage_three.modifiers.move_speed_percent_delta = -20;

                StackStage stage_four{};
                stage_four.minimum_stacks = 4;
                stage_four.modifiers.max_mana_percent_delta = -100;

                StackStage stage_five{};
                stage_five.minimum_stacks = 5;
                stage_five.modifiers.max_hp_percent_delta = -100;

                definition.stack_stages.push_back(stage_one);
                definition.stack_stages.push_back(stage_two);
                definition.stack_stages.push_back(stage_three);
                definition.stack_stages.push_back(stage_four);
                definition.stack_stages.push_back(stage_five);
                return definition;
            }

            [[nodiscard]] inline auto make_negative_ailments_catalog() -> Catalog
            {
                Catalog catalog{};
                catalog.insert(make_poison_definition());
                catalog.insert(make_burn_definition());
                catalog.insert(make_bleed_definition());
                catalog.insert(make_freeze_definition());
                catalog.insert(make_daze_definition());
                catalog.insert(make_silence_definition());
                catalog.insert(make_root_definition());
                catalog.insert(make_curse_definition());
                catalog.insert(make_taunt_definition());
                return catalog;
            }

            [[nodiscard]] inline auto make_status_catalog() -> Catalog
            {
                Catalog catalog{};
                const auto negative_catalog = make_negative_ailments_catalog();
                const auto positive_catalog = make_positive_buffs_catalog();

                for (const auto kind : { Kind::poison, Kind::burn, Kind::bleed, Kind::freeze, Kind::daze, Kind::silence, Kind::root, Kind::curse, Kind::taunt })
                {
                    if (const auto* definition = negative_catalog.find(kind); definition != nullptr)
                    {
                        catalog.insert(*definition);
                    }
                }

                for (const auto kind : { Kind::shield, Kind::haste, Kind::regeneration, Kind::bless, Kind::resistant })
                {
                    if (const auto* definition = positive_catalog.find(kind); definition != nullptr)
                    {
                        catalog.insert(*definition);
                    }
                }

                return catalog;
            }

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
                            current.modifiers = instance.modifiers;
                            current.stack_stages = instance.stack_stages;
                            if (instance.kind == Kind::shield)
                            {
                                current.shield_layers.insert(current.shield_layers.end(), instance.shield_layers.begin(), instance.shield_layers.end());
                            }
                            return;
                        }

                        current.stacks = std::max(current.stacks, instance.stacks);
                        current.applied_at = instance.applied_at;
                        current.expires_at = instance.expires_at;
                        current.next_tick_at = instance.next_tick_at;
                        current.source_entity_id = instance.source_entity_id;
                        current.source_species_id = instance.source_species_id;
                        current.source_template_id = instance.source_template_id;
                        current.modifiers = instance.modifiers;
                        current.stack_stages = instance.stack_stages;
                        return;
                    }

                    instances_.push_back(instance);
                }

                [[nodiscard]] auto accumulate(
                    const Definition& definition,
                    std::uint32_t amount,
                    time::TimePoint now,
                    std::int32_t build_up_threshold_percent_delta = 0,
                    std::int32_t build_up_decay_per_second_percent_delta = 0,
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
                    advance_meter(meter, now, build_up_decay_per_second_percent_delta);

                    const auto safe_threshold_percent = std::max<std::int32_t>(0, 100 + build_up_threshold_percent_delta);
                    const auto effective_threshold = std::max<std::uint32_t>(1, static_cast<std::uint32_t>((meter.threshold * safe_threshold_percent) / 100));

                    meter.current = std::min(meter.capacity, meter.current + amount);
                    meter.last_updated_at = now;

                    if (meter.current < effective_threshold)
                    {
                        return std::nullopt;
                    }

                    auto proc_instance = make_instance(definition, now, 1, source_entity_id, source_species_id, source_template_id);
                    if (!definition.keep_build_up_after_activate)
                    {
                        remove_pending(definition.kind);
                    }
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

                auto decay_build_up(time::TimePoint now, std::int32_t build_up_decay_per_second_percent_delta = 0) -> bool
                {
                    bool changed = false;
                    auto meter_it = meters_.begin();
                    while (meter_it != meters_.end())
                    {
                        auto& meter = *meter_it;
                        if (advance_meter(meter, now, build_up_decay_per_second_percent_delta))
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

                auto sweep(time::TimePoint now, std::int32_t build_up_decay_per_second_percent_delta = 0) -> bool
                {
                    const auto removed_instances = remove_expired(now);
                    const auto decay_changed = decay_build_up(now, build_up_decay_per_second_percent_delta);
                    return !removed_instances.empty() || decay_changed;
                }

                [[nodiscard]] auto aggregate() const -> Totals
                {
                    Totals totals{};
                    std::vector<const ShieldLayer*> shield_layers;

                    for (const auto& instance : instances_)
                    {
                        if (instance.kind == Kind::shield)
                        {
                            for (const auto& layer : instance.shield_layers)
                            {
                                totals.shield_absorb_percent_of_max_hp += layer.absorb_percent_of_max_hp;
                                shield_layers.push_back(&layer);
                            }

                            continue;
                        }

                        totals.modifiers = add_modifiers(totals.modifiers, instance.modifiers);
                        for (const auto& stage : instance.stack_stages)
                        {
                            if (instance.stacks >= stage.minimum_stacks)
                            {
                                totals.modifiers = add_modifiers(totals.modifiers, stage.modifiers);
                            }
                        }
                    }

                    totals.shield_absorb_percent_of_max_hp = std::min<std::uint32_t>(totals.shield_absorb_percent_of_max_hp, 30);

                    if (!shield_layers.empty())
                    {
                        std::sort(shield_layers.begin(), shield_layers.end(), [](const ShieldLayer* lhs, const ShieldLayer* rhs) {
                            return lhs->absorb_percent_of_max_hp > rhs->absorb_percent_of_max_hp;
                        });

                        for (const auto* layer : shield_layers)
                        {
                            if (layer->has_bonus)
                            {
                                totals.modifiers = add_modifiers(totals.modifiers, layer->bonus_modifiers);
                                break;
                            }
                        }
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
                    meter.capacity = std::max<std::uint32_t>(meter.threshold, (meter.threshold * std::max<std::uint32_t>(1, definition.build_up_cap_percent)) / 100);
                    meter.decay_per_second = definition.build_up_decay_per_second;
                    meter.decay_delay = definition.build_up_decay_delay;
                    meter.source_entity_id = source_entity_id;
                    meter.source_species_id = source_species_id;
                    meter.source_template_id = source_template_id;
                    return meter;
                }

                auto advance_meter(Meter& meter, time::TimePoint now, std::int32_t build_up_decay_per_second_percent_delta = 0) -> bool
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
                    const auto safe_decay_percent = std::max<std::int32_t>(0, 100 + build_up_decay_per_second_percent_delta);
                    const auto effective_decay_per_second = static_cast<std::uint32_t>((meter.decay_per_second * safe_decay_percent) / 100);
                    const auto decay_amount = static_cast<std::uint32_t>((decay_window.count() * effective_decay_per_second) / 1000);
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
