
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
#include <intrin.h>     //  __debugbreak
#elif !defined(__clang__) && !defined(__GNUC__)
#include <cstdlib>      //  std::abort
#endif

#include "debug/debug.hpp"
#include "platform/platform_defines.hpp"

//#if PLATFORM_WINDOWS
#include "platform/windows_include.hpp"
#include <iostream>     //  std::vsnprintf()
//#endif

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

static std::atomic<std::uint32_t> s_debug_output_ordinal{ 0u };

void debug_output(const char* format, ...) noexcept
{
    char buffer[1024];
    bool success = false;
    const std::uint32_t ordinal = s_debug_output_ordinal.fetch_add(1u, std::memory_order_relaxed);
    int offset = std::snprintf(buffer, sizeof(buffer), "[%06u] ", ordinal);
    if ((offset >= 0) && (offset < static_cast<int>(sizeof(buffer))))
    {
        va_list args;
        va_start(args, format);
        const int written = std::vsnprintf((buffer + offset), (sizeof(buffer) - static_cast<std::size_t>(offset)), format, args);
        if (written >= 0)
        {
            success = true;
        }
        va_end(args);
    }

//#if PLATFORM_WINDOWS

    ::OutputDebugStringA(success ? buffer : "[debug output format failure]\n");

//#else
// 
//    (void)format;
// 
//#endif
}

}   //  namespace debug_utils
