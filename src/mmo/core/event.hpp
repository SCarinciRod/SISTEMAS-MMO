#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace event
        {
            enum class Type : std::uint8_t
            {
                evolution_due = 0,
                migration_completed,
                zone_wake,
                zone_sleep,
                region_notice
            };

            struct Event
            {
                Type type{ Type::region_notice };
                time::TimePoint due_at{};
                id::EntityId entity_id{ id::invalid_entity_id };
                id::ZoneId zone_id{ id::invalid_zone_id };
                std::uint32_t counter{ 0 };
            };

            class Scheduler
            {
            public:
                auto schedule(const Event& event) -> void
                {
                    events_.emplace(event.due_at, event);
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

            private:
                std::multimap<time::TimePoint, Event> events_;
            };
        }
    }
}
