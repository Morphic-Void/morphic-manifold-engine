//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  MorphicTests-local runtime types. These fixtures deliberately model the
//  properties required by Core tests without depending on Host-local types.

#pragma once

#ifndef MORPHIC_TEST_LOCAL_TYPES_HPP_INCLUDED
#define MORPHIC_TEST_LOCAL_TYPES_HPP_INCLUDED

#include <cstdint>

#include "assets/asset_repository.hpp"

namespace test_environment
{

class CTestRuntime;

struct STestTgaFileLoadState
{
    std::int32_t executive_slot;
    CAssetId request;
};

struct STestTgaDecodeState
{
    std::int32_t executive_slot;
    CAssetId loaded_file;
};

struct STestTgaEncodeState
{
    std::int32_t executive_slot;
    CAssetId source;
    CAssetId request;
};

struct STestTgaFileSaveState
{
    std::int32_t executive_slot;
    CAssetId encoded_file;
    CAssetId request;
};

}   //  namespace test_environment

#endif  //  MORPHIC_TEST_LOCAL_TYPES_HPP_INCLUDED
