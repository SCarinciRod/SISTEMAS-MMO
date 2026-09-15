#pragma once

#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <variant>
#include <vector>
#include "mmo/core/event.hpp"

namespace mmo::world::scheduled
{
    struct ActionId { std::uint64_t value{ 0 }; };
    struct CompleteMigration { core::id::EntityId entity_id; core::id::ZoneId destination_zone_id; };
    struct WakeZone { core::id::ZoneId zone_id; };
    struct SleepZone { core::id::ZoneId zone_id; };
    struct EvolutionDue { core::id::EntityId entity_id; };
    struct RegionNotice { core::id::ZoneId zone_id; std::uint32_t notice_id; };
    using Payload = std::variant<CompleteMigration, WakeZone, SleepZone, EvolutionDue, RegionNotice>;

    struct ScheduledAction
    {
        ActionId id;
        core::time::TimePoint due_at;
        Payload payload;
    };

    enum class Rejection { none, invalid_id, invalid_payload, duplicate_id, entity_missing, transition_rejected, unsupported };
    struct Result
    {
        ActionId id;
        Rejection reason{ Rejection::none };
        [[nodiscard]] auto accepted() const noexcept -> bool { return reason == Rejection::none; }
    };

    [[nodiscard]] inline auto validate(const ScheduledAction& action) -> Rejection
    {
        if (action.id.value == 0) return Rejection::invalid_id;
        const bool valid = std::visit([](const auto& payload)
        {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, CompleteMigration>)
                return payload.entity_id != 0 && payload.destination_zone_id != 0;
            else if constexpr (std::is_same_v<T, EvolutionDue>)
                return payload.entity_id != 0;
            else return payload.zone_id != 0;
        }, action.payload);
        return valid ? Rejection::none : Rejection::invalid_payload;
    }

    class Scheduler
    {
    public:
        [[nodiscard]] auto try_schedule(const ScheduledAction& action) -> Result
        {
            const auto error = validate(action);
            if (error != Rejection::none) return { action.id, error };
            if (ids_.count(action.id.value)) return { action.id, Rejection::duplicate_id };
            ids_.insert(action.id.value);
            try
            {
                actions_.emplace(std::make_pair(action.due_at, action.id.value), action);
            }
            catch (...)
            {
                ids_.erase(action.id.value);
                throw;
            }
            return { action.id };
        }
        [[nodiscard]] auto pop_ready(core::time::TimePoint now) -> std::vector<ScheduledAction>
        {
            std::vector<ScheduledAction> ready;
            while (!actions_.empty() && actions_.begin()->first.first <= now)
            {
                const auto it = actions_.begin();
                ready.push_back(it->second);
                ids_.erase(it->second.id.value);
                actions_.erase(it);
            }
            return ready;
        }
        [[nodiscard]] auto size() const noexcept -> std::size_t { return actions_.size(); }
        [[nodiscard]] auto empty() const noexcept -> bool { return actions_.empty(); }
        [[nodiscard]] auto next_due() const -> std::optional<core::time::TimePoint>
        {
            if (actions_.empty()) return std::nullopt;
            return actions_.begin()->first.first;
        }
    private:
        std::map<std::pair<core::time::TimePoint, std::uint64_t>, ScheduledAction> actions_;
        std::set<std::uint64_t> ids_;
    };

    // Input-only compatibility. Legacy instructions never become domain facts by conversion.
    [[nodiscard]] inline auto from_legacy(const core::event::Event& event, ActionId id)
        -> std::optional<ScheduledAction>
    {
        if (!core::event::is_valid(event)) return std::nullopt;
        switch (event.type)
        {
            case core::event::Type::migration_completed:
                return ScheduledAction{ id, event.due_at, CompleteMigration{ event.entity_id, event.zone_id } };
            case core::event::Type::zone_wake:
                return ScheduledAction{ id, event.due_at, WakeZone{ event.zone_id } };
            case core::event::Type::zone_sleep:
                return ScheduledAction{ id, event.due_at, SleepZone{ event.zone_id } };
            case core::event::Type::evolution_due:
                return ScheduledAction{ id, event.due_at, EvolutionDue{ event.entity_id } };
            case core::event::Type::region_notice:
                return ScheduledAction{ id, event.due_at, RegionNotice{ event.zone_id, event.counter } };
        }
        return std::nullopt;
    }
}
