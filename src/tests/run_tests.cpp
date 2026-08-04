
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   run_tests.cpp
//  Author: Ritchie Brannan
//  Date:   24 Apr 26

#include "tests/run_tests.hpp"
#include "tests/ByteBuffers_test_suite.hpp"
#include "tests/CMemoryToken_test_suite.hpp"
#include "tests/CMemoryView_test_suite.hpp"
#include "tests/DebugService_test_suite.hpp"
#include "tests/ErasedOwner_test_suite.hpp"
#include "tests/StringBuffers_test_suite.hpp"
#include "tests/TInstance_test_suite.hpp"
#include "tests/TOrderedCollection_test_suite.hpp"
#include "tests/TPodFifo_test_suite.hpp"
#include "tests/TPodVector_test_suite.hpp"
#include "tests/TUnorderedCollection_test_suite.hpp"
#include "tests/TOrderedSlots_test_harness.hpp"
#include "tests/TUnorderedSlots_test_harness.hpp"
#include "tests/TQueueTransport_test_suite.hpp"
#include "tests/TMpmcTransport_test_suite.hpp"
#include "tests/TRingTransport_test_suite.hpp"
#include "tests/TOwningTransport_test_suite.hpp"

int run_tests()
{
    int cumulative_result = 0;

    int memory_token_test_result = run_memory_token_tests();
    cumulative_result += memory_token_test_result;

    int memory_view_test_result = run_memory_view_tests();
    cumulative_result += memory_view_test_result;

    int erased_owner_test_result = run_erased_owner_tests();
    cumulative_result += erased_owner_test_result;

    int pod_vector_test_result = run_pod_vector_tests();
    cumulative_result += pod_vector_test_result;

    int byte_buffer_test_result = run_byte_buffer_tests();
    cumulative_result += byte_buffer_test_result;

    int instance_test_result = run_instance_tests();
    cumulative_result += instance_test_result;

    int debug_service_test_result = run_debug_service_tests();
    cumulative_result += debug_service_test_result;

    int pod_fifo_test_result = run_pod_fifo_tests();
    cumulative_result += pod_fifo_test_result;

    int string_buffer_test_result = run_string_buffer_tests();
    cumulative_result += string_buffer_test_result;

    int ordered_collection_test_result = run_ordered_collection_tests();
    cumulative_result += ordered_collection_test_result;

    int unordered_collection_test_result = run_unordered_collection_tests();
    cumulative_result += unordered_collection_test_result;

    int towning_test_result = run_owning_transport_tests();
    cumulative_result += towning_test_result;

    int tqueue_test_result = run_queue_transport_tests();
    cumulative_result += tqueue_test_result;

    int tring_test_result = run_ring_transport_tests();
    cumulative_result += tring_test_result;

    int mpmc_transport_test_result = run_mpmc_transport_tests();
    cumulative_result += mpmc_transport_test_result;

    TOrderedConfig tlex_cfg;
    tlex_cfg.run_fuzz_lightweight = true;
    int tlex_test_result = run_all_tests(tlex_cfg);
    cumulative_result += tlex_test_result;

    TUnorderedConfig tun_cfg;
    //tun_cfg.run_fuzz = true;
    int tun_test_result = run_all_tests(tun_cfg);
    cumulative_result += tun_test_result;

    return cumulative_result;
}
