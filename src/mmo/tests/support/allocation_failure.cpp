#include "allocation_failure.hpp"
#include <cstdlib>
#include <new>

namespace
{
    bool enabled = false;
    std::size_t remaining = 0;
}
namespace mmo::tests::support
{
    auto fail_allocation_after(std::size_t count) noexcept -> void
    {
        remaining = count;
        enabled = true;
    }
    auto disable_allocation_failure() noexcept -> void { enabled = false; }
}

// Separate translation unit keeps replacement allocation/deallocation paired under optimization.
auto operator new(std::size_t size) -> void*
{
    if (enabled)
    {
        if (remaining == 0)
        {
            enabled = false;
            throw std::bad_alloc{};
        }
        --remaining;
    }
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}
auto operator new[](std::size_t size) -> void* { return ::operator new(size); }
auto operator delete(void* memory) noexcept -> void { std::free(memory); }
auto operator delete[](void* memory) noexcept -> void { ::operator delete(memory); }
auto operator delete(void* memory, std::size_t) noexcept -> void { ::operator delete(memory); }
auto operator delete[](void* memory, std::size_t) noexcept -> void { ::operator delete(memory); }
