
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   ManifoldEngine.cpp
//  Author: Ritchie Brannan
//  Date:   24 Apr 26
//
//  The main() function.
//  This is the entry point for the host thread.
//  Program execution begins and ends here.

#include <iostream>

//  These includes are only here to check the compile state:
#include "algo/validate_permutations.hpp"
#include "bit_utils/bit_ops.hpp"
#include "bit_utils/spatial_codes.hpp"
#include "containers/slots/SlotsRankMap.hpp"
#include "containers/slots/TOrderedSlots.hpp"
#include "containers/slots/TUnorderedSlots.hpp"
#include "containers/ByteBuffers.hpp"
#include "containers/StringBuffers.hpp"
#include "containers/TInstance.hpp"
#include "containers/TPodVector.hpp"
#include "containers/TStableStorage.hpp"
#include "containers/TOrderedCollection.hpp"
#include "containers/TUnorderedCollection.hpp"
#include "debug/debug.hpp"
#include "image/codec/tga.hpp"
#include "memory/host_allocator.hpp"
#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "memory/memory_typeless.hpp"
#include "platform/filesystem/internal/file_utils.hpp"
#include "platform/filesystem/file.hpp"
#include "platform/filesystem/log.hpp"
#include "platform/module/binding.hpp"
#include "platform/path/native_path.hpp"
#include "platform/threading/platform_threading.hpp"
#include "system/system_ids.hpp"
#include "threading/transports/bundles/TBulkBundle.hpp"
#include "threading/transports/bundles/TRingBundle.hpp"
#include "threading/transports/bundles/TQueueBundle.hpp"
#include "threading/transports/bundles/TOwningBundle.hpp"
#include "types/fp16data_t.hpp"

#include "tests/run_tests.hpp"

int main()
{
    int ret = 0;
    constexpr std::size_t executable_host_system_id = system_ids::make_system_id(module_ids::executable, thread_ids::host);
    memory::IAllocator* host_allocator = memory::get_the_host_allocator(executable_host_system_id);
    if (MV_FAIL_SAFE_ASSERT(host_allocator != nullptr))
    {
        if (MV_FAIL_SAFE_ASSERT(memory::set_allocator(host_allocator)))
        {
            ret = run_tests();
        }
    }
    return ret;
}

