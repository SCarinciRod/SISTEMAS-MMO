#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "action.hpp"
#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace combat
        {
            enum class TargetKind : std::uint8_t
            {
                none = 0,
                self,
                single
            };

            enum class ValidationCode : std::uint8_t
            {
                ok = 0,
                actor_dead,
                insufficient_health,
                insufficient_mana,
                missing_target,
                invalid_target,
                target_dead,
                target_wrong_zone,
                target_is_self
            };

            struct Cost
            {
                std::int32_t health{ 0 };
                std::int32_t mana{ 0 };
            };

            struct TargetRule
            {
                TargetKind kind{ TargetKind::none };
                bool requires_target{ false };
                bool allow_self{ false };
                bool require_same_zone{ true };
                bool require_alive_target{ true };
                bool allow_forced_target{ true };
            };

            struct AggroRule
            {
                bool generates_threat{ false };
                std::uint32_t threat_on_hit{ 0 };
                std::uint32_t threat_on_whiff{ 0 };
                bool taunt{ false };
                std::optional<time::Milliseconds> forced_target_duration{};
            };

            struct Profile
            {
                action::Kind kind{ action::Kind::idle };
                std::string_view name{};
                Cost cost{};
                TargetRule target{};
                AggroRule aggro{};
            };

            class Catalog
            {
            public:
                auto insert(const Profile& profile) -> bool
                {
                    auto [it, inserted] = profiles_.emplace(profile.kind, profile);
                    (void)it;
                    return inserted;
                }

                auto erase(action::Kind kind) -> bool
                {
                    return profiles_.erase(kind) > 0;
                }

                [[nodiscard]] auto find(action::Kind kind) -> Profile*
                {
                    auto profile_it = profiles_.find(kind);
                    if (profile_it == profiles_.end())
                    {
                        return nullptr;
                    }

                    return &profile_it->second;
                }

                [[nodiscard]] auto find(action::Kind kind) const -> const Profile*
                {
                    auto profile_it = profiles_.find(kind);
                    if (profile_it == profiles_.end())
                    {
                        return nullptr;
                    }

                    return &profile_it->second;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return profiles_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return profiles_.empty();
                }

            private:
                std::unordered_map<action::Kind, Profile> profiles_;
            };

            struct Intent
            {
                action::Kind kind{ action::Kind::idle };
                std::optional<id::EntityId> target_entity_id{};
                time::TimePoint issued_at{};
            };

            struct ActorView
            {
                id::EntityId entity_id{ id::invalid_entity_id };
                id::ZoneId zone_id{ id::invalid_zone_id };
                bool alive{ true };
                std::int32_t health_current{ 0 };
                std::int32_t mana_current{ 0 };
                std::optional<id::EntityId> forced_target_id{};
            };

            struct TargetView
            {
                id::EntityId entity_id{ id::invalid_entity_id };
                id::ZoneId zone_id{ id::invalid_zone_id };
                bool alive{ true };
            };

            struct Validation
            {
                ValidationCode code{ ValidationCode::ok };
                std::optional<id::EntityId> resolved_target_entity_id{};
                bool target_forced{ false };

                [[nodiscard]] auto ok() const noexcept -> bool
                {
                    return code == ValidationCode::ok;
                }
            };

            struct Resolution
            {
                Intent intent{};
                Profile profile{};
                Validation validation{};
                std::optional<id::EntityId> final_target_entity_id{};
                Cost paid{};
                std::uint32_t planned_threat{ 0 };
            };

            struct ThreatEntry
            {
                id::EntityId source_entity_id{ id::invalid_entity_id };
                std::uint32_t threat{ 0 };
                time::TimePoint last_updated_at{};
            };

            class AggroState
            {
            public:
                auto add_threat(id::EntityId source_entity_id, std::uint32_t amount, time::TimePoint now) -> void
                {
                    if (amount == 0)
                    {
                        return;
                    }

                    auto entry_it = find_entry(source_entity_id);
                    if (entry_it == entries_.end())
                    {
                        entries_.push_back(ThreatEntry{ source_entity_id, amount, now });
                        return;
                    }

                    entry_it->threat += amount;
                    entry_it->last_updated_at = now;
                }

                auto force_target(id::EntityId target_entity_id, time::Milliseconds duration, time::TimePoint now) -> void
                {
                    forced_target_id_ = target_entity_id;
                    forced_target_until_ = now + duration;
                }

                auto clear_forced_target() -> void
                {
                    forced_target_id_.reset();
                    forced_target_until_.reset();
                }

                [[nodiscard]] auto select_target(time::TimePoint now) const -> std::optional<id::EntityId>
                {
                    if (forced_target_id_.has_value())
                    {
                        if (!forced_target_until_.has_value() || now <= forced_target_until_.value())
                        {
                            return forced_target_id_;
                        }
                    }

                    if (entries_.empty())
                    {
                        return std::nullopt;
                    }

                    auto best_entry_it = std::max_element(entries_.begin(), entries_.end(), [](const ThreatEntry& lhs, const ThreatEntry& rhs) {
                        return lhs.threat < rhs.threat;
                    });

                    if (best_entry_it == entries_.end() || best_entry_it->threat == 0)
                    {
                        return std::nullopt;
                    }

                    return best_entry_it->source_entity_id;
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return entries_.empty() && !forced_target_id_.has_value();
                }

            private:
                std::vector<ThreatEntry> entries_;
                std::optional<id::EntityId> forced_target_id_{};
                std::optional<time::TimePoint> forced_target_until_{};

                [[nodiscard]] auto find_entry(id::EntityId source_entity_id) -> std::vector<ThreatEntry>::iterator
                {
                    return std::find_if(entries_.begin(), entries_.end(), [source_entity_id](const ThreatEntry& entry) {
                        return entry.source_entity_id == source_entity_id;
                    });
                }
            };

            [[nodiscard]] inline auto can_pay_cost(const ActorView& actor, const Cost& cost) -> bool
            {
                if (!actor.alive)
                {
                    return false;
                }

                if (actor.health_current < cost.health)
                {
                    return false;
                }

                if (actor.mana_current < cost.mana)
                {
                    return false;
                }

                return true;
            }

            [[nodiscard]] inline auto resolve_target_entity_id(
                const ActorView& actor,
                const Intent& intent,
                const Profile& profile) -> std::optional<id::EntityId>
            {
                if (actor.forced_target_id.has_value() && profile.target.allow_forced_target)
                {
                    return actor.forced_target_id;
                }

                return intent.target_entity_id;
            }

            [[nodiscard]] inline auto validate_target(
                const ActorView& actor,
                const Intent& intent,
                const Profile& profile,
                const std::optional<TargetView>& target) -> Validation
            {
                auto validation = Validation{};
                const auto resolved_target_entity_id = resolve_target_entity_id(actor, intent, profile);

                if (profile.target.requires_target && !resolved_target_entity_id.has_value())
                {
                    validation.code = ValidationCode::missing_target;
                    return validation;
                }

                if (!resolved_target_entity_id.has_value())
                {
                    validation.resolved_target_entity_id.reset();
                    return validation;
                }

                if (!profile.target.allow_self && resolved_target_entity_id.value() == actor.entity_id)
                {
                    validation.code = ValidationCode::target_is_self;
                    return validation;
                }

                if (!target.has_value())
                {
                    validation.code = ValidationCode::missing_target;
                    return validation;
                }

                if (target->entity_id != resolved_target_entity_id.value())
                {
                    validation.code = ValidationCode::invalid_target;
                    return validation;
                }

                if (profile.target.require_same_zone && target->zone_id != actor.zone_id)
                {
                    validation.code = ValidationCode::target_wrong_zone;
                    return validation;
                }

                if (profile.target.require_alive_target && !target->alive)
                {
                    validation.code = ValidationCode::target_dead;
                    return validation;
                }

                validation.code = ValidationCode::ok;
                validation.resolved_target_entity_id = resolved_target_entity_id;
                validation.target_forced = actor.forced_target_id.has_value() && profile.target.allow_forced_target;
                return validation;
            }

            [[nodiscard]] inline auto validate(
                const ActorView& actor,
                const Intent& intent,
                const Profile& profile,
                const std::optional<TargetView>& target) -> Validation
            {
                if (!actor.alive)
                {
                    return Validation{ ValidationCode::actor_dead };
                }

                if (!can_pay_cost(actor, profile.cost))
                {
                    if (actor.health_current < profile.cost.health)
                    {
                        return Validation{ ValidationCode::insufficient_health };
                    }

                    return Validation{ ValidationCode::insufficient_mana };
                }

                return validate_target(actor, intent, profile, target);
            }

            [[nodiscard]] inline auto resolve(
                const ActorView& actor,
                const Intent& intent,
                const Profile& profile,
                const std::optional<TargetView>& target) -> Resolution
            {
                const auto validation = validate(actor, intent, profile, target);
                const auto planned_threat = profile.aggro.generates_threat ? profile.aggro.threat_on_hit : 0;

                return Resolution{
                    intent,
                    profile,
                    validation,
                    validation.resolved_target_entity_id,
                    profile.cost,
                    planned_threat
                };
            }

            [[nodiscard]] inline auto make_auto_attack_profile() -> Profile
            {
                Profile profile{};
                profile.kind = action::Kind::auto_attack;
                profile.name = "Auto Attack";
                profile.cost = Cost{};
                profile.target.kind = TargetKind::single;
                profile.target.requires_target = true;
                profile.target.allow_self = false;
                profile.target.require_same_zone = true;
                profile.target.require_alive_target = true;
                profile.target.allow_forced_target = true;
                profile.aggro.generates_threat = true;
                profile.aggro.threat_on_hit = 100;
                profile.aggro.threat_on_whiff = 20;
                return profile;
            }

            [[nodiscard]] inline auto make_dodge_profile() -> Profile
            {
                Profile profile{};
                profile.kind = action::Kind::dodge;
                profile.name = "Dodge";
                profile.cost = Cost{};
                profile.target.kind = TargetKind::none;
                profile.target.requires_target = false;
                profile.target.allow_self = true;
                profile.target.require_same_zone = true;
                profile.target.require_alive_target = false;
                profile.target.allow_forced_target = false;
                profile.aggro.generates_threat = false;
                return profile;
            }

            [[nodiscard]] inline auto make_skill_focused_catalog() -> Catalog
            {
                Catalog catalog{};
                catalog.insert(make_auto_attack_profile());
                catalog.insert(make_dodge_profile());
                return catalog;
            }
        }
    }
}