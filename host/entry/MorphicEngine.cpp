
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:    MorphicEngine.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    24 Apr 26
//
//  The main() function.
//  This is the entry point for the host thread.
//  Program execution begins and ends here.

#include <cstdint>

#include "host/runtime/host.hpp"
#include "host/system/host_context.hpp"
#include "platform/system/process_priority.hpp"
#include "platform/threading/hw_thread_count.hpp"

int main()
{
    if (!host::host_context_install())
    {
        return 1;
    }
    const int host_result = host::host();
    platform::system::set_current_process_priority(platform::system::EProcessPriority::AboveNormal);
    const std::uint32_t hw_threads_supported = platform::threading::query_hardware_thread_count();
    (void)hw_threads_supported;

    return host_result;
}
