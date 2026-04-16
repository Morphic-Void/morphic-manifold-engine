
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   debug.hpp
//  Author: Ritchie Brannan
//  Date:   16 Apr 26
//
//  Barebones debugging utilities

#pragma once

#ifndef DEBUG_HPP_INCLUDED
#define DEBUG_HPP_INCLUDED

namespace debug_utils
{

//==============================================================================
//  Debug enabling definition
//==============================================================================

#ifndef MV_DEBUG_BUILD 
    #if defined(_DEBUG) || !defined(NDEBUG)
        #define MV_DEBUG_BUILD  1
    #else
        #define MV_DEBUG_BUILD  0
    #endif
#endif  //  #ifndef MV_DEBUG_BUILD

//==============================================================================
//  Debug configuration
//==============================================================================

bool enable_asserts(const bool enable = true) noexcept;

inline bool disable_asserts() noexcept
{
    return enable_asserts(false);
}

//==============================================================================
//  Debug trapping
//==============================================================================

void hard_fail() noexcept;
bool fail_safe(const bool success) noexcept;

//==============================================================================
//  Debug macros
//==============================================================================

#if MV_DEBUG_BUILD 

//  Debugging assert macros
#define MV_HARD_ASSERT(x) do { if (!(x)) debug_utils::hard_fail(); } while (0)
#define MV_SELF_ASSERT(x) ((void)(x))
#define MV_FAIL_SAFE_ASSERT(x) (debug_utils::fail_safe((x)))

#else

//  Stubbed assert macros
#define MV_HARD_ASSERT(x) do {} while (0)
#define MV_SELF_ASSERT(x) ((void)0)
#define MV_FAIL_SAFE_ASSERT(x) ((x))

#endif

}   //  namespace debug_utils

#endif  //  #ifndef DEBUG_HPP_INCLUDED
