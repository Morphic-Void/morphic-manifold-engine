
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   run_tests.cpp
//  Author: Ritchie Brannan
//  Date:   24 Apr 26

#include "tests/run_tests.hpp"
#include "tests/TOrderedSlots_test_harness.hpp"
#include "tests/TUnorderedSlots_test_harness.hpp"
#include "tests/TQueueTransport_test_suite.hpp"
#include "tests/TRingTransport_test_suite.hpp"
#include "tests/TOwningTransport_test_suite.hpp"

int run_tests()
{
    int towning_test_result = run_owning_transport_tests();

    int tqueue_test_result = run_queue_transport_tests();

    int tring_test_result = run_ring_transport_tests();

    TOrderedConfig tlex_cfg;
    tlex_cfg.run_fuzz_lightweight = true;
    int tlex_test_result = run_all_tests(tlex_cfg);

    TUnorderedConfig tun_cfg;
    //tun_cfg.run_fuzz = true;
    int tun_test_result = run_all_tests(tun_cfg);

    return 0;
}