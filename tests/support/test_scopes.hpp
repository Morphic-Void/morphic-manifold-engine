//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#pragma once

#ifndef MORPHIC_TEST_SCOPES_HPP_INCLUDED
#define MORPHIC_TEST_SCOPES_HPP_INCLUDED

#include "memory/memory_context.hpp"
#include "system/system_context.hpp"

namespace tests
{

class TMemoryContextScope
{
public:
    explicit TMemoryContextScope(memory::CMemoryContext* const context) noexcept
        : m_previous(memory::set_thread_memory_context(context)) {}
    ~TMemoryContextScope() noexcept
    {
        (void)memory::set_thread_memory_context(m_previous);
    }
    TMemoryContextScope(const TMemoryContextScope&) = delete;
    TMemoryContextScope& operator=(const TMemoryContextScope&) = delete;

private:
    memory::CMemoryContext* m_previous;
};

class TModuleIdScope
{
public:
    explicit TModuleIdScope(const module_ids::id_type id) noexcept
        : m_previous(system_context::set_ambient_module_id(id)) {}
    ~TModuleIdScope() noexcept
    {
        (void)system_context::set_ambient_module_id(m_previous);
    }
    TModuleIdScope(const TModuleIdScope&) = delete;
    TModuleIdScope& operator=(const TModuleIdScope&) = delete;

private:
    module_ids::id_type m_previous;
};

}   //  namespace tests

#endif  //  MORPHIC_TEST_SCOPES_HPP_INCLUDED
