
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  ManifoldEngine.cpp
// 
//  This file contains the 'main' function. Program execution begins and ends there.

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
#include "types/fp16data_t.hpp"
#include "io/file/file.hpp"
#include "io/file/log.hpp"
#include "debug/debug.hpp"

#include "tests/TOrderedSlots_test_harness.hpp"
#include "tests/TUnorderedSlots_test_harness.hpp"

int main()
{
    TOrderedConfig tlex_cfg;
    tlex_cfg.run_fuzz_lightweight = true;
    int tlex_result = run_all_tests(tlex_cfg);

    TUnorderedConfig tun_cfg;
    //tun_cfg.run_fuzz = true;
    int tun_result = run_all_tests(tun_cfg);

    return tlex_result == 0 ? tun_result : tlex_result;
}

