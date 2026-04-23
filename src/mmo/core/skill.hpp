#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "action.hpp"
#include "combat.hpp"
#include "id.hpp"
#include "status.hpp"
#include "time.hpp"
#include "trigger.hpp"

namespace mmo
{
    namespace core
    {
        namespace skill
        {
            enum class Mode : std::uint8_t
            {
                active = 0,
                passive,
                trigger
            };

            enum class Pattern : std::uint8_t
            {
                chain = 0,
                initial,
                finisher,
                evolve
            };

            struct Tags
            {
                Mode mode{ Mode::active };
                Pattern pattern{ Pattern::initial };
                bool animation_cancel_allowed{ false };
                bool enabled_by_default{ true };
            };

            struct Identity
            {
                id::SkillId skill_id{ id::invalid_skill_id };
                std::string_view name{};
                std::string_view description{};
            };

            struct Gate
            {
                std::vector<trigger::Requirement> trigger_requirements{};
                std::vector<status::Kind> blocked_by_statuses{};
            };

            struct Execution
            {
                std::optional<action::Kind> action_kind{};
                std::optional<combat::Profile> combat_profile{};
                std::optional<time::Milliseconds> cooldown{};
                std::optional<time::Milliseconds> channel{};
                std::optional<time::Milliseconds> recovery{};
            };

            struct Relations
            {
                std::vector<id::SkillId> previous_skill_ids{};
                std::vector<id::SkillId> next_skill_ids{};
                std::vector<id::SkillId> fusion_component_skill_ids{};
                std::vector<id::SkillId> evolve_source_skill_ids{};
            };

            struct Definition
            {
                Identity identity{};
                Tags tags{};
                Gate gate{};
                Execution execution{};
                Relations relations{};
            };

            struct State
            {
                id::SkillId skill_id{ id::invalid_skill_id };
                bool learned{ false };
                bool equipped{ false };
                bool enabled{ true };
                std::uint32_t rank{ 1 };
                time::TimePoint learned_at{};
                std::optional<time::TimePoint> last_used_at{};
                std::optional<time::TimePoint> cooldown_ready_at{};
            };

            class Catalog
            {
            public:
                auto insert(const Definition& definition) -> bool
                {
                    if (definition.identity.skill_id == id::invalid_skill_id)
                    {
                        return false;
                    }

                    auto [it, inserted] = definitions_.emplace(definition.identity.skill_id, definition);
                    (void)it;
                    return inserted;
                }

                auto erase(id::SkillId skill_id) -> bool
                {
                    return definitions_.erase(skill_id) > 0;
                }

                [[nodiscard]] auto find(id::SkillId skill_id) -> Definition*
                {
                    auto definition_it = definitions_.find(skill_id);
                    if (definition_it == definitions_.end())
                    {
                        return nullptr;
                    }

                    return &definition_it->second;
                }

                [[nodiscard]] auto find(id::SkillId skill_id) const -> const Definition*
                {
                    auto definition_it = definitions_.find(skill_id);
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
                std::unordered_map<id::SkillId, Definition> definitions_;
            };

            [[nodiscard]] constexpr auto make_tags(Mode mode, Pattern pattern, bool animation_cancel_allowed = false, bool enabled_by_default = true) -> Tags
            {
                return Tags{ mode, pattern, animation_cancel_allowed, enabled_by_default };
            }

            [[nodiscard]] constexpr auto make_active_tags(Pattern pattern = Pattern::initial, bool animation_cancel_allowed = true) -> Tags
            {
                return make_tags(Mode::active, pattern, animation_cancel_allowed, true);
            }

            [[nodiscard]] constexpr auto make_passive_tags(Pattern pattern = Pattern::initial) -> Tags
            {
                return make_tags(Mode::passive, pattern, false, true);
            }

            [[nodiscard]] constexpr auto make_trigger_tags(Pattern pattern = Pattern::chain) -> Tags
            {
                return make_tags(Mode::trigger, pattern, false, false);
            }

            [[nodiscard]] inline auto is_active(const Definition& definition) -> bool
            {
                return definition.tags.mode == Mode::active;
            }

            [[nodiscard]] inline auto is_passive(const Definition& definition) -> bool
            {
                return definition.tags.mode == Mode::passive;
            }

            [[nodiscard]] inline auto is_trigger(const Definition& definition) -> bool
            {
                return definition.tags.mode == Mode::trigger;
            }

            [[nodiscard]] inline auto is_chain(const Definition& definition) -> bool
            {
                return definition.tags.pattern == Pattern::chain;
            }

            [[nodiscard]] inline auto is_initial(const Definition& definition) -> bool
            {
                return definition.tags.pattern == Pattern::initial;
            }

            [[nodiscard]] inline auto is_finisher(const Definition& definition) -> bool
            {
                return definition.tags.pattern == Pattern::finisher;
            }

            [[nodiscard]] inline auto is_evolve(const Definition& definition) -> bool
            {
                return definition.tags.pattern == Pattern::evolve;
            }

            [[nodiscard]] inline auto make_skill_catalog() -> Catalog
            {
                return Catalog{};
            }
        }
    }
}