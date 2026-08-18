
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
#include "containers/TPodFifo.hpp"
#include "containers/TPodOrderedSlots.hpp"
#include "containers/TPodUnorderedSlots.hpp"
#include "containers/TPodVector.hpp"
#include "containers/TOrderedCollection.hpp"
#include "containers/TUnorderedCollection.hpp"
#include "debug/macros.hpp"
#include "host/runtime/host.hpp"
#include "host/system/host_context.hpp"
#include "image/codec/tga.hpp"
#include "memory/memory_token.hpp"
#include "platform/filesystem/file.hpp"
#include "platform/filesystem/log.hpp"
#include "platform/module/binding.hpp"
#include "platform/path/native_path.hpp"
#include "platform/system/performance_counter.hpp"
#include "platform/system/process_priority.hpp"
#include "platform/threading/platform_threading.hpp"
#include "system/erased_owner.hpp"
#include "system/erased_pod.hpp"
#include "system/system_ids.hpp"
#include "system/system_type_registration.hpp"
#include "system/TStaticLookup.hpp"
#include "system/transported_types.hpp"
#include "threading/threading.hpp"
#include "threading/CParkingGate.hpp"
#include "types/fp16data_t.hpp"

#include "tests/run_tests.hpp"

//  Retained for later extraction into a standalone TGA test; the current host
//  runtime exercises the module-driven TGA flow instead.
bool test_tga()
{
    bool success = false;
    CByteBuffer loaded_tga = platform::filesystem::loadFile("test_data/input/files/test_input.tga");
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
                success = platform::filesystem::saveFile("test_data/output/files/test_output.tga", encoded_tga.const_view());
            }
        }
    }
    return success;
}

int main(const int argc, char** const argv)
{
    if (should_print_usage(argc, argv))
    {
        print_usage();
        return 0;
    }

    if (!host::host_context_install())
    {
        return 1;
    }
    const int host_result = host::host();
    platform::system::set_current_process_priority(platform::system::EProcessPriority::AboveNormal);
    const std::uint32_t hw_threads_supported = platform::threading::query_hardware_thread_count();
    (void)hw_threads_supported;

    const ETestRunMode test_run_mode = parse_test_run_mode(argc, argv);
    const int test_result = run_tests(test_run_mode);
    return (host_result != 0) ? host_result : test_result;
}
