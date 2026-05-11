#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace event
        {
            // Event contract: types and validation rules for entity/zone requirements.
            enum class Type : std::uint8_t
            {
                evolution_due = 0,
                migration_completed,
                zone_wake,
                zone_sleep,
                region_notice
            };

            enum class ValidationIssue : std::uint8_t
            {
                none = 0,
                missing_entity,
                missing_zone
            };

            struct Event
            {
                Type type{ Type::region_notice };
                time::TimePoint due_at{};
                id::EntityId entity_id{ id::invalid_entity_id };
                id::ZoneId zone_id{ id::invalid_zone_id };
                std::uint32_t counter{ 0 };
            };

            [[nodiscard]] constexpr auto requires_entity(Type type) -> bool
            {
                switch (type)
                {
                    case Type::evolution_due:
                    case Type::migration_completed:
                        return true;

                    case Type::zone_wake:
                    case Type::zone_sleep:
                    case Type::region_notice:
                    default:
                        return false;
                }
            }

            [[nodiscard]] constexpr auto requires_zone(Type type) -> bool
            {
                switch (type)
                {
                    case Type::migration_completed:
                    case Type::zone_wake:
                    case Type::zone_sleep:
                    case Type::region_notice:
                        return true;

                    case Type::evolution_due:
                    default:
                        return false;
                }
            }

            // Validation checks enforce required IDs based on event type.
            [[nodiscard]] constexpr auto validate(const Event& event) -> ValidationIssue
            {
                if (requires_entity(event.type) && event.entity_id == id::invalid_entity_id)
                {
                    return ValidationIssue::missing_entity;
                }

                if (requires_zone(event.type) && event.zone_id == id::invalid_zone_id)
                {
                    return ValidationIssue::missing_zone;
                }

                return ValidationIssue::none;
            }

            [[nodiscard]] constexpr auto is_valid(const Event& event) -> bool
            {
                return validate(event) == ValidationIssue::none;
            }

            [[nodiscard]] inline auto type_name(Type type) -> std::string_view
            {
                switch (type)
                {
                    case Type::evolution_due:
                        return "evolution_due";
                    case Type::migration_completed:
                        return "migration_completed";
                    case Type::zone_wake:
                        return "zone_wake";
                    case Type::zone_sleep:
                        return "zone_sleep";
                    case Type::region_notice:
                        return "region_notice";
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
                    case ValidationIssue::missing_entity:
                        return "missing_entity";
                    case ValidationIssue::missing_zone:
                        return "missing_zone";
                    default:
                        return "unknown";
                }
            }

            // Scheduler orders events by due time and exposes ready batches.
            class Scheduler
            {
            public:
                auto schedule(const Event& event) -> void
                {
                    events_.emplace(event.due_at, event);
                }

                [[nodiscard]] auto try_schedule(const Event& event) -> bool
                {
                    if (!is_valid(event))
                    {
                        return false;
                    }

                    schedule(event);
                    return true;
                }

                [[nodiscard]] auto pop_ready(time::TimePoint now) -> std::vector<Event>
                {
                    std::vector<Event> ready_events;

                    while (!events_.empty())
                    {
                        auto first_it = events_.begin();

                        if (first_it->first > now)
                        {
                            break;
                        }

                        ready_events.push_back(first_it->second);
                        events_.erase(first_it);
                    }

                    return ready_events;
                }

                [[nodiscard]] auto count_ready(time::TimePoint now) const -> std::size_t
                {
                    std::size_t count = 0;

                    for (auto it = events_.begin(); it != events_.end(); ++it)
                    {
                        if (it->first > now)
                        {
                            break;
                        }

                        ++count;
                    }

                    return count;
                }

                [[nodiscard]] auto next_due() const -> std::optional<time::TimePoint>
                {
                    if (events_.empty())
                    {
                        return std::nullopt;
                    }

                    return events_.begin()->first;
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return events_.empty();
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return events_.size();
                }

                auto clear() -> void
                {
                    events_.clear();
                }

            private:
                std::multimap<time::TimePoint, Event> events_;
            };
        }
    }
}