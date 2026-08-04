
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   debug.cpp
//  Author: Ritchie Brannan
//  Date:   16 Apr 26
//
//  Barebones debugging utilities

#include <atomic>       //  std::atomic
#include "debug/debug.hpp"

namespace debug_utils
{

static std::atomic<bool> s_asserts_enabled{ true };

bool enable_asserts(const bool enable) noexcept
{
    return s_asserts_enabled.exchange(enable, std::memory_order_relaxed);
}

}   //  namespace debug_utils
