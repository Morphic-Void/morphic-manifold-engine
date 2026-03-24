
//  File:   validate_permutations.hpp
//  Author: Ritchie Brannan
//  Date:   10 Mar 26
//
//  Permutation validation (C++17, noexcept).
//
//  Purpose:
// 
//      Side-effect free noexcept permutation validation.
//
//      O(N) alternatives may be preferable if side-effects and/or
//      mutability and/or memory allocation are acceptable.
//

//==============================================================================
//  Validate that an array contains every value in [base_value, base_value + count)
//  exactly once.
//
//  Template parameters:
//      TElement    - element type stored in the array
//      TInternal   - internal arithmetic type (defaults to TElement)
//      ValueBits   - semantic width of the value domain
//
//  Requirements:
//      * TElement and TInternal must be unsigned integer types
//      * ValueBits must fit within both TElement and TInternal
//
//  Method:
//      * range-check all values are within [base_value, base_value + count)
//      * repeatedly split validated value ranges in half
//      * count the population of the lower half only
//      * infer the upper-half population by subtraction
//      * defer non-trivial upper halves for later processing
//
//  Complexity:
//      Time:   O(N log N)
//      Space:  O(ValueBits) stack
//==============================================================================

#pragma once

#ifndef VALIDATE_PERMUTATIONS_HPP_INCLUDED
#define VALIDATE_PERMUTATIONS_HPP_INCLUDED

#include <cstdint>
#include <limits>

namespace algo
{

template<
    typename TElement = std::size_t,
    typename TInternal = TElement,
    std::size_t ValueBits = std::numeric_limits<std::conditional_t<(sizeof(TElement) <= sizeof(TInternal)), TElement, TInternal>>::digits>
inline bool validate_permutations(const TElement* const values, const TInternal count, const TInternal base_value = TInternal{ 0 }) noexcept
{
    //--------------------------------------------------------------------------
    //  Compile-time constraints
    //--------------------------------------------------------------------------

    static_assert((std::numeric_limits<TElement>::is_integer && !std::numeric_limits<TElement>::is_signed && !std::is_same_v<TElement, bool>), "TElement must be an unsigned integer type");
    static_assert((std::numeric_limits<TInternal>::is_integer && !std::numeric_limits<TInternal>::is_signed && !std::is_same_v<TInternal, bool>), "TInternal must be an unsigned integer type");

    static_assert(ValueBits > 0u, "ValueBits must be at least 1");
    static_assert(ValueBits <= std::numeric_limits<TElement>::digits, "ValueBits exceeds TElement bit capacity");
    static_assert(ValueBits <= std::numeric_limits<TInternal>::digits, "ValueBits exceeds TInternal bit capacity");

    //--------------------------------------------------------------------------
    //  Derived limits
    //--------------------------------------------------------------------------

    constexpr TInternal k_max_elements = (TInternal{ 1 } << (ValueBits - 1u));
    constexpr std::size_t k_deferred_capacity = ValueBits;

    //--------------------------------------------------------------------------
    //  Basic sanity checks
    //--------------------------------------------------------------------------

    if ((values == nullptr) && (count != TInternal{ 0 }))
    {
        return false;
    }

    if ((count > k_max_elements) || (base_value > k_max_elements) || (count > (k_max_elements - base_value)))
    {
        return false;
    }

    if (count <= TInternal{ 1 })
    {
        return true;
    }

    //--------------------------------------------------------------------------
    //  Initial range validation
    //--------------------------------------------------------------------------

    for (TInternal i = TInternal{ 0 }; i < count; ++i)
    {
        const TInternal x = static_cast<TInternal>(values[i]);

        if ((x - base_value) >= count)
        {
            return false;
        }
    }

    //--------------------------------------------------------------------------
    //  Deferred interval stack
    //--------------------------------------------------------------------------

    struct value_range_t
    {
        TInternal lo;
        TInternal hi;
    };

    value_range_t deferred[k_deferred_capacity];
    std::size_t deferred_count = 0u;

    //  Seed traversal with the full validated range [base_value, base_value + count).
    deferred[deferred_count++] = { base_value, static_cast<TInternal>(base_value + count) };

    //--------------------------------------------------------------------------
    //  Depth-first traversal of value interval tree
    //--------------------------------------------------------------------------

    while (deferred_count > 0u)
    {
        const value_range_t range = deferred[--deferred_count];

        const TInternal lo = range.lo;
        TInternal hi = range.hi;
        TInternal width = hi - lo;

        //  Descend the left spine of this subtree. After the shift, width is
        //  the expected population of the lower half [lo, lo + width).
        while (width > TInternal{ 1 })
        {
            width >>= 1u;

            //  actual_width counts how many elements actually fall in the
            //  lower-half range [lo, lo + width).
            TInternal actual_width = TInternal{ 0 };

            for (TInternal i = TInternal{ 0 }; i < count; ++i)
            {
                const TInternal x = static_cast<TInternal>(values[i]);

                if ((x - lo) < width)
                {
                    ++actual_width;
                }
            }

            if (actual_width != width)
            {
                return false;
            }

            const TInternal mid = static_cast<TInternal>(lo + width);

            if ((hi - mid) > TInternal{ 1 })
            {
                deferred[deferred_count++] = { mid, hi };
            }

            hi = mid;
        }
    }

    return true;
}

}   //  namespace algo

#endif  //  VALIDATE_PERMUTATIONS_HPP_INCLUDED