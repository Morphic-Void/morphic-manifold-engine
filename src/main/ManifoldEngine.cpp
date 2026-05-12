
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
#include "platform/system/performance_counter.hpp"
#include "platform/system/process_priority.hpp"
#include "platform/threading/platform_threading.hpp"
#include "system/system_ids.hpp"
#include "system/TStaticLookup.hpp"
#include "threading/transports/bundles/TBulkBundle.hpp"
#include "threading/transports/bundles/TRingBundle.hpp"
#include "threading/transports/bundles/TQueueBundle.hpp"
#include "threading/transports/bundles/TOwningBundle.hpp"
#include "threading/CParkingGate.hpp"
#include "types/fp16data_t.hpp"

#include "tests/run_tests.hpp"

bool test_tga()
{
    bool success = false;
    CByteBuffer loaded_tga = platform::filesystem::loadFile("d:/test_input.tga");
    if (!loaded_tga.is_empty())
    {
        image::codec::tga::decoded_image_desc desc;
        CByteRectBuffer decoded_tga = image::codec::tga::decode(loaded_tga.const_view(), desc);
        if (!decoded_tga.is_empty())
        {
            image::codec::tga::EncodeOptions options;
            CByteBuffer encoded_tga = image::codec::tga::encode(decoded_tga.const_view(), options);
            if (!encoded_tga.is_empty())
            {
                success = platform::filesystem::saveFile("d:/test_output.tga", encoded_tga.const_view());
            }
        }
    }
    return success;
}

int main()
{
    int ret = 0;
    memory::IAllocator* host_allocator = memory::get_the_host_allocator(system_ids::host);
    if (MV_FAIL_SAFE_ASSERT(host_allocator != nullptr))
    {
        if (MV_FAIL_SAFE_ASSERT(memory::set_allocator(host_allocator)))
        {
            //test_tga();
            platform::system::set_current_process_priority(platform::system::ProcessPriority::AboveNormal);
            const std::uint32_t hw_threads_supported = platform::threading::query_hardware_thread_count();
            ret = run_tests();
        }
    }
    return ret;
}

