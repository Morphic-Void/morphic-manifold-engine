
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   system_context.hpp
//  Author: Ritchie Brannan
//  Date:   12 Jul 26
//
//  Ambient system context.

#pragma once

#ifndef SYSTEM_CONTEXT_HPP_INCLUDED
#define SYSTEM_CONTEXT_HPP_INCLUDED

#include <cstddef>      //  std::size_t

namespace system_context
{

//==============================================================================
//  Ambient system identity
//==============================================================================

[[nodiscard]] std::size_t get_ambient_module_id() noexcept;
[[nodiscard]] std::size_t get_ambient_thread_id() noexcept;
[[nodiscard]] std::size_t get_ambient_system_id() noexcept;

std::size_t set_ambient_module_id(std::size_t module_id = 0u) noexcept;
std::size_t set_ambient_thread_id(std::size_t thread_id = 0u) noexcept;

}   //  namespace system_context

#endif  //  #ifndef SYSTEM_CONTEXT_HPP_INCLUDED
