
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   system_context.cpp
//  Author: Ritchie Brannan
//  Date:   12 Jul 26
//
//  Module-local and thread-local ambient system context state.

#include <cstddef>      //  std::size_t

#include "system/system_context.hpp"
#include "system/system_ids.hpp"

namespace system_context
{

//==============================================================================
//  Ambient state
//==============================================================================

static std::size_t s_module_id{ 0u };

thread_local std::size_t t_thread_id{ 0u };

//==============================================================================
//  Ambient system identity
//==============================================================================

std::size_t get_ambient_module_id() noexcept
{
    return s_module_id;
}

std::size_t get_ambient_thread_id() noexcept
{
    return t_thread_id;
}

std::size_t get_ambient_system_id() noexcept
{
    return system_ids::make_system_id(s_module_id, t_thread_id);
}

std::size_t set_ambient_module_id(const std::size_t module_id) noexcept
{
    const std::size_t previous_module_id = s_module_id;
    s_module_id = module_id;
    return previous_module_id;
}

std::size_t set_ambient_thread_id(const std::size_t thread_id) noexcept
{
    const std::size_t previous_thread_id = t_thread_id;
    t_thread_id = thread_id;
    return previous_thread_id;
}

}   //  namespace system_context
