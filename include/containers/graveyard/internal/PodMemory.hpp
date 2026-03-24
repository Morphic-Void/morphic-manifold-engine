//  File:   PodMemory.hpp
//  Author: Ritchie Brannan
//  Date:   17 Feb 26
//
//  POD memory substrate for container utilities (C++17, noexcept).
//
//  Purpose
//  -------
//  Provides the foundational memory layer shared by the POD container family.
//
//  This header contains:
//
//      - Shared limits and limit helpers
//      - Growth policies and default growth policy
//      - Allocation alignment policy
//      - Aligned allocation / deallocation (nothrow)
//
//  Scope
//  -----
//  This file is strictly mechanical. It defines no container semantics.
//
//  Design Constraints
//  ------------------
//  - Requires C++17 or greater.
//  - T must be trivially copyable where applicable.
//  - No exceptions (allocation uses nothrow new[]).
//  - 32-bit element domain cap:
//      pod_memory::k_max_elements = 0x80000000u
//
//  Higher-level container headers (arrays, vectors, buffers) build semantic
//  behavior on top of the primitives defined here.
//

#pragma once

#ifndef POD_MEMORY_HPP_INCLUDED
#define POD_MEMORY_HPP_INCLUDED

#include <algorithm>    //  std::max, std::min
#include <cstddef>      //  std::size_t, alignof
#include <limits>       //  std::numeric_limits
#include <new>          //  std::align_val_t, std::nothrow, aligned operator new[]/delete[]
#include <type_traits>  //  std::is_trivially_copyable

#include "bit_utils/bit_ops.hpp"

namespace pod_memory
{

//==============================================================================
//  Shared limits
//==============================================================================

static_assert(sizeof(std::size_t) >= sizeof(std::uint32_t), "PodMemory requires std::size_t to be at least 32-bit.");

//  *** DO NOT INCREASE THIS ***
//  allows the full positive signed 32-bit index range
constexpr std::size_t k_max_elements = 0x80000000u;

//  element_size == 0 : returns the domain cap (used when size is not relevant)
constexpr std::size_t max_elements(const std::size_t element_size = 0u) noexcept
{
    return (element_size == 0u) ? k_max_elements :
        std::min((std::numeric_limits<std::size_t>::max() / element_size), k_max_elements);
}

template<class T> constexpr std::size_t t_max_elements() noexcept { return max_elements(sizeof(T)); }

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
    return (required + pow2_stepping + (pow2_stepping >> 1) - 1u) & (0u - pow2_stepping);
}

//  capped_growth_rate_curve(required, pow2_stepping)
//
//  Returns the minimum of the geometric base_growth_curve() and the stepped base_stepped_growth()
//  recommendation curves.
//
//  This makes pow2_stepping the maximum step between successive recommended capacity classes.
inline std::size_t capped_growth_rate_curve(const std::size_t required, const std::size_t pow2_stepping) noexcept
{   //  pow2_stepping must be a power of 2 and >= 2.
    return std::min(base_growth_curve(required), base_stepped_growth(required, pow2_stepping));
}

inline std::size_t array_growth_policy(const std::size_t required) noexcept
{   //  default policy when additional capacity is required for automatic array growth
    return std::max(capped_growth_rate_curve(required, std::size_t{ 1024 }), std::size_t{ 32 });
}

inline std::size_t vector_growth_policy(const std::size_t required) noexcept
{   //  default policy when additional capacity is required for automatic vector growth
    return std::max(capped_growth_rate_curve(required, std::size_t{ 1024 }), std::size_t{ 32 });
}

inline std::size_t buffer_growth_policy(const std::size_t required) noexcept
{   //  default policy when additional capacity is required for automatic buffer growth
    return std::max(capped_growth_rate_curve(required, std::size_t{ 65536 }), std::size_t{ 4096 });
}

inline std::size_t default_growth_policy(const std::size_t required) noexcept
{   //  default policy when additional capacity is required with no capped growth rate
    return std::max(base_growth_curve(required), std::size_t{ 32 });
}

//==============================================================================
//  Allocation alignment policy
//==============================================================================

constexpr std::size_t byte_alignment_policy(const std::size_t align) noexcept
{   //  apply the alignment policy in BYTES.
    return std::max(bit_ops::align_to_pow2(align), std::size_t{ alignof(void*) });
}

constexpr std::align_val_t alignment_policy(const std::size_t align) noexcept
{   //  apply the alignment policy and cast the type
    return std::align_val_t{ byte_alignment_policy(align) };
}

//==============================================================================
//  Byte memory allocation and deallocation
//
//  Note:
//
//      These allocators will become wrappers for a memory service
//      shared between all DLLs.
//==============================================================================

inline void* byte_allocate(const std::size_t bytes, const std::size_t align) noexcept
{
    return (bytes != 0u) ? ::operator new[](bytes, alignment_policy(align), std::nothrow) : nullptr;
}

inline void byte_deallocate(void* const ptr, const std::size_t align) noexcept
{
    if (ptr != nullptr)
    {   //  note: the null check is not strictly required
        ::operator delete[](ptr, alignment_policy(align));
    }
}

inline void byte_deallocate(const void* const ptr, const std::size_t align) noexcept
{
    byte_deallocate(const_cast<void*>(ptr), align);
}

//==============================================================================
//  Typed memory allocation and deallocation
//==============================================================================

template<typename T>
inline T* t_allocate(const std::size_t count) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value,
        "pod_memory::t_allocate requires trivially copyable T.");

    constexpr std::size_t k_max_count = t_max_elements<T>();
    if (count > k_max_count)
    {
        return nullptr;
    }

    //  Safe: count <= max_elements ensures no overflow in multiplication
    const std::size_t bytes = sizeof(T) * count;

    return static_cast<T*>(byte_allocate(bytes, alignof(T)));
}

template<typename T>
inline void t_deallocate(T* const ptr) noexcept
{
    static_assert(std::is_trivially_copyable<T>::value,
        "pod_memory::t_deallocate requires trivially copyable T.");

    byte_deallocate(static_cast<void*>(ptr), alignof(T));
}

}   //  namespace pod_memory

#endif  //  #ifndef POD_MEMORY_HPP_INCLUDED
