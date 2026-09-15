#pragma once
#include <cstddef>

namespace mmo::tests::support
{
    // Linked only into the atomic-mutation executable, never the server.
    auto fail_allocation_after(std::size_t successful_allocations) noexcept -> void;
    auto disable_allocation_failure() noexcept -> void;

    class AllocationFailure
    {
    public:
        explicit AllocationFailure(std::size_t successful_allocations) noexcept
        {
            fail_allocation_after(successful_allocations);
        }
        ~AllocationFailure() { disable_allocation_failure(); }
        AllocationFailure(const AllocationFailure&) = delete;
        auto operator=(const AllocationFailure&) -> AllocationFailure& = delete;
    };
}
