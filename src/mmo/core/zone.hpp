#pragma once

#include <cstdint>
#include <unordered_map>

#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace core
    {
        namespace zone
        {
            struct State
            {
                id::ZoneId zone_id{ id::invalid_zone_id };
                bool active{ false };
                std::uint32_t player_count{ 0 };
                std::uint32_t entity_count{ 0 };
                time::TickCount last_tick{ 0 };
                time::TimePoint last_player_activity{};
            };

            class Table
            {
            public:
                auto ensure(id::ZoneId zone_id) -> State&
                {
                    auto insert_result = zones_.emplace(zone_id, State{});
                    auto& it = insert_result.first;
                    const bool inserted = insert_result.second;
                    if (inserted)
                    {
                        it->second.zone_id = zone_id;
                    }

                    return it->second;
                }

                auto get(id::ZoneId zone_id) -> State*
                {
                    auto zone_it = zones_.find(zone_id);
                    if (zone_it == zones_.end())
                    {
                        return nullptr;
                    }

                    return &zone_it->second;
                }

                auto get(id::ZoneId zone_id) const -> const State*
                {
                    auto zone_it = zones_.find(zone_id);
                    if (zone_it == zones_.end())
                    {
                        return nullptr;
                    }

                    return &zone_it->second;
                }

                auto set_player_count(id::ZoneId zone_id, std::uint32_t count, time::TimePoint now) -> State&
                {
                    auto& zone_state = ensure(zone_id);
                    zone_state.player_count = count;
                    zone_state.active = count > 0;

                    if (count > 0)
                    {
                        zone_state.last_player_activity = now;
                    }

                    return zone_state;
                }

                auto set_entity_count(id::ZoneId zone_id, std::uint32_t count) -> State&
                {
                    auto& zone_state = ensure(zone_id);
                    zone_state.entity_count = count;
                    return zone_state;
                }

                auto mark_tick(id::ZoneId zone_id, time::TickCount tick) -> State&
                {
                    auto& zone_state = ensure(zone_id);
                    zone_state.last_tick = tick;
                    return zone_state;
                }

                [[nodiscard]] auto should_tick_full(id::ZoneId zone_id) const -> bool
                {
                    auto zone_it = zones_.find(zone_id);
                    if (zone_it == zones_.end())
                    {
                        return false;
                    }

                    return zone_it->second.player_count > 0;
                }

            private:
                std::unordered_map<id::ZoneId, State> zones_;
            };
        }
    }
}
