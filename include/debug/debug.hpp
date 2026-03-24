
//  File:   debug.hpp
//  Author: Ritchie Brannan
//  Date:   17 Feb 26
//
//  Barebones debugging utilities
//

#pragma once

#ifndef DEBUG_HPP_INCLUDED
#define DEBUG_HPP_INCLUDED

#if defined(_MSC_VER)
#include <intrin.h> // __debugbreak
#elif !defined(__clang__) && !defined(__GNUC__)
#include <cstdlib>  // std::abort
#endif

//==============================================================================
//  Debug enabling definition
//==============================================================================

#ifndef VE_DEBUG_BUILD 
    #if defined(_DEBUG) || !defined(NDEBUG)
        #define VE_DEBUG_BUILD  1
    #else
        #define VE_DEBUG_BUILD  0
    #endif
#endif  //  #ifndef VE_DEBUG_BUILD

//==============================================================================
//  Debug trap
//==============================================================================

#if VE_DEBUG_BUILD 

namespace debug_utils
{

inline void hard_fail() noexcept
{
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__clang__) || defined(__GNUC__)
    __builtin_trap();
#else
    std::abort();
#endif
}

inline bool fail_safe(const bool ok) noexcept
{
    if (!ok)
    {
        hard_fail();
    }
    return ok;
}

}   //  namespace debug_utils

#endif  //  #endif VE_DEBUG_BUILD 

//==============================================================================
//  Debug macros
//==============================================================================

#if VE_DEBUG_BUILD 

//  Debugging assert macros
#define VE_HARD_ASSERT(x) do { if (!(x)) debug_utils::hard_fail(); } while (0)
#define VE_SELF_ASSERT(x) ((void)(x))
#define VE_FAIL_SAFE_ASSERT(x) (debug_utils::fail_safe((x)))

#else

//  Stubbed assert macros
#define VE_HARD_ASSERT(x) do {} while (0)
#define VE_SELF_ASSERT(x) ((void)0)
#define VE_FAIL_SAFE_ASSERT(x) ((x))

#endif

#endif  //  #ifndef DEBUG_HPP_INCLUDED
