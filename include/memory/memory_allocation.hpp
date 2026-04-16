
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   memory_allocation.hpp
//  Author: Ritchie Brannan
//  Date:   17 Feb 26
//
//  The memory substrate (C++17, noexcept).
//
//  Purpose
//  -------
//  Provides the shared foundational memory layer.
//
//  This header contains:
//
//      - Shared limits and limit helpers
//      - Growth policies and default growth policy
//      - Allocation alignment policy
//      - Allocation configuration (nothrow)
//      - Aligned allocation / deallocation (nothrow)
//      - Allocator interface definition
//
//  Scope
//  -----
//  This file is strictly mechanical. It defines no semantics.
//
//  Design Constraints
//  ------------------
//  - Requires C++17 or greater.
//  - No exceptions (allocation uses nothrow new[]).
//  - 32-bit element domain cap:
//      memory::k_max_elements = 0x80000000u
//
//  Higher-level structures build semantic behavior on top of the
//  primitives defined here.

#pragma once

#ifndef MEMORY_ALLOCATION_HPP_INCLUDED
#define MEMORY_ALLOCATION_HPP_INCLUDED

#include <algorithm>    //  std::min, std::max
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint32_t
#include <limits>       //  std::numeric_limits
#include <new>          //  std::align_val_t, std::nothrow, aligned operator new[]/delete[]
#include <type_traits>  //  std::is_same_v

#include "bit_utils/bit_ops.hpp"
#include "debug/debug.hpp"

namespace memory
{

//==============================================================================
//  Shared limits
//==============================================================================

static_assert((std::is_same_v<std::size_t, std::uint32_t> || std::is_same_v<std::size_t, std::uint64_t>),
    "memory_allocation.hpp requires std::size_t to be either std::uint32_t or std::uint64_t.");

//  *** DO NOT INCREASE THIS ***
//  allows the full positive signed 32-bit index range
constexpr std::size_t k_max_elements = 0x80000000u;

//  element_size == 0 : returns the domain cap (used when size is not relevant)
[[nodiscard]] constexpr std::size_t max_elements(const std::size_t element_size = 0u) noexcept
{
    return std::min((std::numeric_limits<std::size_t>::max() / std::max(element_size, std::size_t{ 1 })), k_max_elements);
}

[[nodiscard]] constexpr bool in_non_empty_range(const std::size_t x, const std::size_t max) noexcept
{
    return (x - 1u) < max;
}

//==============================================================================
//  Growth policies
//==============================================================================

//  Naming convention used throughout growth policies:
//
//    capacity    = current container capacity
//    required    = minimum capacity required after growth
//    recommended = capacity recommended by the growth curve

//  base_growth_curve(required)
//
//  Produces a ~1.5x geometric growth curve by recommending the next
//  capacity class from the repeating pattern 2 -> 3 -> 4 scaled by
//  the highest power of two <= 'required'.
inline std::size_t base_growth_curve(const std::size_t required) noexcept
{   //  bucketed ~1.5x growth policy
    std::size_t recommended = k_max_elements;
    if (required < recommended)
    {
        recommended = 2u;
        if (required >= recommended)
        {
            std::size_t base = bit_ops::hi_bit_mask(required);
            std::size_t step = base >> 1;
            std::size_t half = step;
            if (required & (step - 1u))
            {
                if (required & step)
                {
                    step <<= 1;
                }
                half >>= 1;
            }
            if (required & half)
            {
                step <<= 1;
            }
            recommended = std::min((base + step), k_max_elements);
        }
    }
    return recommended;
}

//  base_stepped_growth(required, pow2_stepping)
//
//  Returns the first pow2_stepping-spaced capacity class whose midpoint is greater than 'required'.
//
//  Equivalent to rounding (required + pow2_stepping / 2) up to the next multiple of pow2_stepping.
inline std::size_t base_stepped_growth(const std::size_t required, const std::size_t pow2_stepping) noexcept
{   //  pow2_stepping must be a power of 2 and >= 2.
    MV_HARD_ASSERT((pow2_stepping >= 2) && bit_ops::is_pow2(pow2_stepping));
    return (required + pow2_stepping + (pow2_stepping >> 1) - 1u) & ~(pow2_stepping - 1u);
}

//  capped_growth_rate_curve(required, pow2_stepping)
//
//  Returns the minimum of the geometric base_growth_curve() and the stepped base_stepped_growth()
//  recommendation curves.
//
//  This makes pow2_stepping the maximum step between successive recommended capacity classes.
inline std::size_t capped_growth_rate_curve(const std::size_t required, const std::size_t pow2_stepping) noexcept
{   //  pow2_stepping must be a power of 2 and >= 2.
    MV_HARD_ASSERT((pow2_stepping >= 2) && bit_ops::is_pow2(pow2_stepping));
    return std::min(base_growth_curve(required), base_stepped_growth(required, pow2_stepping));
}

constexpr std::size_t k_buffer_growth_policy_capped_rate{ 65536u };
constexpr std::size_t k_buffer_growth_policy_min_capacity{ 4096u };

inline std::size_t buffer_growth_policy(const std::size_t required) noexcept
{   //  default policy when additional capacity is required for automatic buffer growth
    return std::max(capped_growth_rate_curve(required, k_buffer_growth_policy_min_capacity), k_buffer_growth_policy_min_capacity);
}

constexpr std::size_t k_vector_growth_policy_capped_rate{ 1024u };
constexpr std::size_t k_vector_growth_policy_min_capacity{ 32u };

inline std::size_t vector_growth_policy(const std::size_t required) noexcept
{   //  default policy when additional capacity is required for automatic vector growth
    return std::max(capped_growth_rate_curve(required, k_vector_growth_policy_capped_rate), k_vector_growth_policy_min_capacity);
}

constexpr std::size_t k_default_growth_policy_min_capacity{ 32u };

inline std::size_t default_growth_policy(const std::size_t required) noexcept
{   //  default policy when additional capacity is required with no capped growth rate
    return std::max(base_growth_curve(required), k_default_growth_policy_min_capacity);
}

//==============================================================================
//  Allocation alignment policy
//==============================================================================

constexpr std::size_t byte_alignment_policy(const std::size_t align) noexcept
{   //  apply the alignment policy in bytes.
    return std::max(bit_ops::reduce_alignment_to_pow2(align), std::size_t{ alignof(void*) });
}

constexpr std::align_val_t alignment_policy(const std::size_t align) noexcept
{   //  apply the alignment policy and cast the type
    return std::align_val_t{ byte_alignment_policy(align) };
}

//==============================================================================
//  Allocation configuration
//  - thread-safe at the substrate level
//  - allocator replacement is rejected while allocations remain live
//  - allocation disabling affects allocation only; deallocation remains enabled
//  - allocation enable/disable is intended primarily for test/debug control
//==============================================================================

class IAllocator
{
public:
    virtual void* byte_allocate(const std::size_t bytes, const std::size_t align) noexcept = 0;
    virtual void byte_deallocate(void* const ptr, const std::size_t align) noexcept = 0;
};

//  Installs the active allocator.
//  Returns false if allocator replacement is currently disallowed.
bool set_allocator(IAllocator* allocator) noexcept;

//  Sets allocation enable state and returns the previous state.
bool enable_allocation(const bool enable = true) noexcept;

inline bool disable_allocation() noexcept
{
    return enable_allocation(false);
}

//==============================================================================
//  Byte memory allocation and deallocation
//  - thread-safe at the substrate level
//  - supports DLL-spanning allocation routing when a shared allocator is installed
//==============================================================================

//  Allocates a byte block using the active allocator.
//  Zero-size allocation requests are rejected and return nullptr.
void* byte_allocate(const std::size_t bytes, const std::size_t align) noexcept;

//  Deallocates a byte block using the active allocator.
//  Null pointers are accepted and ignored.
void byte_deallocate(void* const ptr, const std::size_t align) noexcept;

inline void byte_deallocate(const void* const ptr, const std::size_t align) noexcept
{
    byte_deallocate(const_cast<void*>(ptr), align);
}

//==============================================================================
//  Typed allocation limit
//==============================================================================

template<class T>
constexpr std::size_t t_max_elements() noexcept { return max_elements(sizeof(T)); }

//==============================================================================
//  Typed default alignment
//  Note:: alignment policy can apply a higher floor
//==============================================================================

template<class T>
constexpr std::size_t t_default_align() noexcept
{
    constexpr std::size_t align = sizeof(T);
    return std::max((align & ~(align - 1u)), alignof(T));
}

//==============================================================================
//  Typed memory allocation and deallocation
//==============================================================================

template<typename T>
inline T* t_allocate(const std::size_t count) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value,
        "memory::t_allocate requires trivially copyable T.");

    constexpr std::size_t k_max_count = t_max_elements<T>();
    if ((count - 1u) >= k_max_count)
    {   //  the allocation request is either too large or zero size
        return nullptr;
    }

    //  Safe: count <= max_elements ensures no byte count overflow
    const std::size_t bytes = sizeof(T) * count;

    return static_cast<T*>(byte_allocate(bytes, t_default_align<T>()));
}

template<typename T>
inline void t_deallocate(T* const ptr) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value,
        "memory::t_deallocate requires trivially copyable T.");

    byte_deallocate(static_cast<void*>(ptr), t_default_align<T>());
}

//==============================================================================
//  Allocator interface definition
//==============================================================================


}   //  namespace memory

#endif  //  #ifndef MEMORY_ALLOCATION_HPP_INCLUDED
