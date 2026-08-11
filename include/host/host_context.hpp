
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   host_context.hpp
//  Author: Ritchie Brannan
//  Date:   13 July 26
//
//  Installs the host context including the host memory context.
// 
//  This file should not be included in modules/DLLs.
//
//  Design constraints
//  ------------------
//  - Requires C++17 or later.
//  - No exceptions.

#pragma once

#ifndef HOST_CONTEXT_HPP_INCLUDED
#define HOST_CONTEXT_HPP_INCLUDED

//==============================================================================
//  External declarations
//==============================================================================

namespace memory
{
class CMemoryContext;
}

namespace host
{

//  Installs the host context.
void host_context_install() noexcept;
[[nodiscard]] memory::CMemoryContext* host_memory_context() noexcept;

}   //  namespace host

#endif  //  #ifndef HOST_CONTEXT_HPP_INCLUDED
