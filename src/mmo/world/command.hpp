#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "mmo/core/id.hpp"
#include "mmo/core/time.hpp"

namespace mmo
{
    namespace world
    {
        namespace command
        {
            // Assigned by trusted server ingress; a client sequence is not authoritative ordering.
            struct Sequence
            {
                std::uint64_t value{ 0 };
            };

            [[nodiscard]] constexpr auto operator==(Sequence lhs, Sequence rhs) noexcept -> bool
            {
                return lhs.value == rhs.value;
            }

            [[nodiscard]] constexpr auto operator!=(Sequence lhs, Sequence rhs) noexcept -> bool
            {
                return !(lhs == rhs);
            }

            [[nodiscard]] constexpr auto operator<(Sequence lhs, Sequence rhs) noexcept -> bool
            {
                return lhs.value < rhs.value;
            }

            inline constexpr Sequence invalid_sequence{};

            struct MoveToZone
            {
                core::id::ZoneId destination_zone_id{ core::id::invalid_zone_id };
            };

            using CommandId = Sequence;

            struct AdjustHealth
            {
                std::int32_t delta{ 0 };
            };

            // Trusted server grant of one item.
            struct AddInventoryItem
            {
                core::id::ItemId item_id{ core::id::invalid_item_id };
                core::id::ItemTemplateId template_id{ core::id::invalid_item_template_id };
            };

            using Payload = std::variant<MoveToZone, AdjustHealth, AddInventoryItem>;

            struct Envelope
            {
                core::time::TickCount target_tick{ 0 };
                Sequence sequence{};
                core::id::EntityId actor{ core::id::invalid_entity_id };
                Payload payload{};
            };

            enum class IngressError : std::uint8_t
            {
                none = 0,
                queue_full,
                duplicate_sequence,
                too_old,
                too_far_in_future,
                invalid_actor,
                invalid_sequence
            };

            struct EnqueueResult
            {
                IngressError error{ IngressError::none };

                [[nodiscard]] constexpr auto accepted() const noexcept -> bool
                {
                    return error == IngressError::none;
                }
            };

            struct IngressRejection
            {
                core::time::TickCount target_tick{ 0 };
                Sequence sequence{};
                IngressError error{ IngressError::none };
            };

            struct InboxConfig
            {
                std::size_t capacity{ 1'024 };
                core::time::TickCount maximum_future_ticks{ 4 };
            };

            struct InboxMetrics
            {
                std::size_t depth{ 0 };
                std::size_t capacity{ 0 };
                std::uint64_t rejected_full{ 0 };
                std::size_t high_watermark{ 0 };
                std::uint64_t expired_before_capture{ 0 };
            };

            enum class ExecutionRejection : std::uint8_t
            {
                none = 0,
                invalid_actor,
                entity_missing,
                invalid_destination,
                already_in_destination,
                transition_rejected,
                invalid_quantity,
                invalid_item,
                invalid_tick
            };

            struct ExecutionResult
            {
                Sequence sequence{};
                ExecutionRejection reason{ ExecutionRejection::none };

                [[nodiscard]] constexpr auto accepted() const noexcept -> bool
                {
                    return reason == ExecutionRejection::none;
                }
            };

            class Batch
            {
            public:
                using const_iterator = std::vector<Envelope>::const_iterator;

                explicit Batch(core::time::TickCount target_tick = 0) noexcept
                    : target_tick_(target_tick)
                {
                }

                [[nodiscard]] auto target_tick() const noexcept -> core::time::TickCount
                {
                    return target_tick_;
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return commands_.size();
                }

                [[nodiscard]] auto empty() const noexcept -> bool
                {
                    return commands_.empty();
                }

                [[nodiscard]] auto operator[](std::size_t index) const noexcept -> const Envelope&
                {
                    return commands_[index];
                }

                [[nodiscard]] auto begin() const noexcept -> const_iterator
                {
                    return commands_.begin();
                }

                [[nodiscard]] auto end() const noexcept -> const_iterator
                {
                    return commands_.end();
                }

            private:
                Batch(core::time::TickCount target_tick, std::vector<Envelope> commands)
                    : target_tick_(target_tick), commands_(std::move(commands))
                {
                }

                core::time::TickCount target_tick_{ 0 };
                std::vector<Envelope> commands_{};

                friend class Inbox;
            };

            struct CaptureResult
            {
                Batch batch{};
                std::vector<IngressRejection> rejections{};
            };

            class Inbox
            {
            public:
                explicit Inbox(InboxConfig config = {})
                    : config_(config)
                {
                    queued_.reserve(config_.capacity);
                }

                [[nodiscard]] auto try_push(
                    Envelope envelope,
                    core::time::TickCount current_tick) -> EnqueueResult
                {
                    if (envelope.sequence == invalid_sequence)
                    {
                        return { IngressError::invalid_sequence };
                    }

                    if (envelope.actor == core::id::invalid_entity_id)
                    {
                        return { IngressError::invalid_actor };
                    }

                    if (envelope.target_tick < current_tick)
                    {
                        return { IngressError::too_old };
                    }

                    if (envelope.target_tick - current_tick > config_.maximum_future_ticks)
                    {
                        return { IngressError::too_far_in_future };
                    }

                    if (pending_sequences_.count(envelope.sequence) != 0 ||
                        recent_sequences_.count(envelope.sequence) != 0)
                    {
                        return { IngressError::duplicate_sequence };
                    }

                    if (queued_.size() >= config_.capacity)
                    {
                        ++rejected_full_;
                        return { IngressError::queue_full };
                    }

                    pending_sequences_.insert(envelope.sequence);
                    queued_.push_back(std::move(envelope));
                    high_watermark_ = std::max(high_watermark_, queued_.size());
                    return {};
                }

                [[nodiscard]] auto capture_for_tick(
                    core::time::TickCount current_tick) -> CaptureResult
                {
                    std::vector<Envelope> selected;
                    std::vector<Envelope> expired;
                    std::vector<Envelope> retained;
                    selected.reserve(queued_.size());
                    expired.reserve(queued_.size());
                    retained.reserve(queued_.size());

                    for (auto& envelope : queued_)
                    {
                        if (envelope.target_tick < current_tick)
                        {
                            expired.push_back(std::move(envelope));
                        }
                        else if (envelope.target_tick == current_tick)
                        {
                            selected.push_back(std::move(envelope));
                        }
                        else
                        {
                            retained.push_back(std::move(envelope));
                        }
                    }

                    queued_.swap(retained);

                    const auto canonical_less = [](const Envelope& lhs, const Envelope& rhs)
                    {
                        if (lhs.target_tick != rhs.target_tick)
                        {
                            return lhs.target_tick < rhs.target_tick;
                        }

                        return lhs.sequence < rhs.sequence;
                    };
                    std::sort(expired.begin(), expired.end(), canonical_less);
                    std::sort(selected.begin(), selected.end(), canonical_less);

                    std::vector<IngressRejection> rejections;
                    rejections.reserve(expired.size());
                    for (const auto& envelope : expired)
                    {
                        release_and_remember(envelope.sequence);
                        rejections.push_back(IngressRejection{
                            envelope.target_tick,
                            envelope.sequence,
                            IngressError::too_old
                        });
                    }

                    for (const auto& envelope : selected)
                    {
                        release_and_remember(envelope.sequence);
                    }

                    expired_before_capture_ += static_cast<std::uint64_t>(expired.size());
                    return CaptureResult{
                        Batch{ current_tick, std::move(selected) },
                        std::move(rejections)
                    };
                }

                [[nodiscard]] auto size() const noexcept -> std::size_t
                {
                    return queued_.size();
                }

                [[nodiscard]] auto capacity() const noexcept -> std::size_t
                {
                    return config_.capacity;
                }

                [[nodiscard]] auto metrics() const noexcept -> InboxMetrics
                {
                    return InboxMetrics{
                        queued_.size(),
                        config_.capacity,
                        rejected_full_,
                        high_watermark_,
                        expired_before_capture_
                    };
                }

            private:
                InboxConfig config_{};
                std::vector<Envelope> queued_{};
                std::set<Sequence> pending_sequences_{};
                std::deque<Sequence> recent_sequence_order_{};
                std::set<Sequence> recent_sequences_{};
                std::uint64_t rejected_full_{ 0 };
                std::size_t high_watermark_{ 0 };
                std::uint64_t expired_before_capture_{ 0 };

                auto release_and_remember(Sequence sequence) -> void
                {
                    pending_sequences_.erase(sequence);
                    recent_sequences_.insert(sequence);
                    recent_sequence_order_.push_back(sequence);

                    while (recent_sequence_order_.size() > config_.capacity)
                    {
                        const auto oldest = recent_sequence_order_.front();
                        recent_sequence_order_.pop_front();
                        recent_sequences_.erase(oldest);
                    }
                }
            };
        }
    }
}
