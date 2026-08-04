
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

//  Transitional negative-path test control.
bool enable_asserts(const bool enable = true) noexcept;

inline bool disable_asserts() noexcept
{
    return enable_asserts(false);
}

}   //  namespace debug_utils

#include "debug/macros.hpp"

#endif  //  #ifndef DEBUG_HPP_INCLUDED
