
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   debug.cpp
//  Author: Ritchie Brannan
//  Date:   16 Apr 26
//
//  Barebones debugging utilities

#include <atomic>       //  std::atomic

#if defined(_MSC_VER)
#include <intrin.h> // __debugbreak
#elif !defined(__clang__) && !defined(__GNUC__)
#include <cstdlib>  // std::abort
#endif

#include "debug/debug.hpp"

//==============================================================================
//  Debug trapping
//==============================================================================

namespace debug_utils
{

static std::atomic<bool> s_asserts_enabled{ true };

bool enable_asserts(const bool enable) noexcept
{
    return s_asserts_enabled.exchange(enable, std::memory_order_relaxed);
}

void hard_fail() noexcept
{
    if (s_asserts_enabled.load(std::memory_order_acquire))
    {
#if defined(_MSC_VER)
        __debugbreak();
#elif defined(__clang__) || defined(__GNUC__)
        __builtin_trap();
#else
        std::abort();
#endif
    }
}

bool fail_safe(const bool success) noexcept
{
    if (!success)
    {
        hard_fail();
    }
    return success;
}

}   //  namespace debug_utils
