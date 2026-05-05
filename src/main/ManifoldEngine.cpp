
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

//  These includes are only here to check the compile state:
#include <iostream>
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
#include "threading/transports/bundles/TBulkBundle.hpp"
#include "threading/transports/bundles/TRingBundle.hpp"
#include "threading/transports/bundles/TQueueBundle.hpp"
#include "threading/transports/bundles/TOwningBundle.hpp"
#include "platform/filesystem/internal/file_utils.hpp"
#include "platform/filesystem/file.hpp"
#include "platform/filesystem/log.hpp"
#include "platform/module/binding.hpp"
#include "platform/path/native_path.hpp"
#include "platform/threading/primitives.hpp"
#include "system/system_ids.hpp"
#include "image/codec/tga.hpp"
#include "types/fp16data_t.hpp"
#include "debug/debug.hpp"

#include "tests/run_tests.hpp"

int main()
{
    int ret = 0;
    constexpr std::size_t executable_host_system_id = system_ids::make_system_id(module_ids::executable, thread_ids::host);
    if (MV_FAIL_SAFE_ASSERT(memory::install_system_allocator(executable_host_system_id)))
    {
        ret = run_tests();
    }
    return ret;
}

