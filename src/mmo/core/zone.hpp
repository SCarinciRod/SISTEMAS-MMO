#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <vector>

#include "id.hpp"
#include "time.hpp"

namespace mmo
{
    namespace world
    {
        class World;
    }

    namespace core
    {
        namespace zone
        {
            struct State
            {
                id::ZoneId zone_id{ id::invalid_zone_id };
                bool wake_requested{ false };
                std::uint64_t player_count{ 0 };
                std::uint64_t entity_count{ 0 };
                time::TickCount last_tick{ 0 };
                time::TimePoint last_player_activity{};

                [[nodiscard]] auto is_active() const noexcept -> bool
                {
                    return player_count > 0 || wake_requested;
                }
            };

            class Table
            {
            public:
                [[nodiscard]] auto get(id::ZoneId zone_id) const -> const State*
                {
                    const auto zone_it = zones_.find(zone_id);
                    return zone_it == zones_.end() ? nullptr : &zone_it->second;
                }

                [[nodiscard]] auto list_ids() const -> std::vector<id::ZoneId>
                {
                    std::vector<id::ZoneId> ids;
                    ids.reserve(zones_.size());
                    for (const auto& entry : zones_)
                    {
                        ids.push_back(entry.first);
                    }

                    return ids;
                }

            private:
                friend class mmo::world::World;

                std::map<id::ZoneId, State> zones_;

                auto ensure(id::ZoneId zone_id) -> State&
                {
                    auto insert_result = zones_.emplace(zone_id, State{});
                    auto& state = insert_result.first->second;
                    if (insert_result.second)
                    {
                        state.zone_id = zone_id;
                    }

                    return state;
                }

                [[nodiscard]] auto can_remove_entity(
                    id::ZoneId zone_id,
                    bool is_player) const -> bool
                {
                    const auto* state = get(zone_id);
                    return state != nullptr &&
                        state->entity_count > 0 &&
                        (!is_player || state->player_count > 0);
                }

                [[nodiscard]] auto can_add_entity(
                    id::ZoneId zone_id,
                    bool is_player) const -> bool
                {
                    if (zone_id == id::invalid_zone_id)
                    {
                        return false;
                    }

                    const auto* state = get(zone_id);
                    if (state == nullptr)
                    {
                        return true;
                    }

                    return state->entity_count != std::numeric_limits<std::uint64_t>::max() &&
                        (!is_player ||
                            state->player_count != std::numeric_limits<std::uint64_t>::max());
                }

                auto add_entity(
                    id::ZoneId zone_id,
                    bool is_player,
                    time::TimePoint now) -> void
                {
                    auto& state = ensure(zone_id);
                    ++state.entity_count;
                    if (is_player)
                    {
                        ++state.player_count;
                        state.last_player_activity = now;
                    }
                }

                auto remove_entity(
                    id::ZoneId zone_id,
                    bool is_player,
                    time::TimePoint now) -> bool
                {
                    if (!can_remove_entity(zone_id, is_player))
                    {
                        return false;
                    }

                    auto& state = zones_.find(zone_id)->second;
                    --state.entity_count;
                    if (is_player)
                    {
                        --state.player_count;
                        state.last_player_activity = now;
                    }

                    return true;
                }

                auto wake(id::ZoneId zone_id) -> void
                {
                    ensure(zone_id).wake_requested = true;
                }

                auto sleep(id::ZoneId zone_id) -> bool
                {
                    auto& state = ensure(zone_id);
                    if (state.player_count > 0)
                    {
                        return false;
                    }

                    state.wake_requested = false;
                    return true;
                }

                auto mark_tick(id::ZoneId zone_id, time::TickCount tick) -> void
                {
                    ensure(zone_id).last_tick = tick;
                }
            };
        }
    }
}
