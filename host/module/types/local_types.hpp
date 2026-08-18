
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    local_types.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    17 Aug 26
//
//  Complete definitions for host-local runtime types.

#pragma once

#ifndef HOST_LOCAL_TYPES_HPP_INCLUDED
#define HOST_LOCAL_TYPES_HPP_INCLUDED

#include <cstdint>

#include "assets/asset_repository.hpp"
#include "image/codec/tga.hpp"

namespace host
{

class CHost;

struct SHostTgaFileLoadState
{
    std::int32_t executive_slot;
    CAssetId request;
};

struct SHostTgaDecodeState
{
    std::int32_t executive_slot;
    CAssetId loaded_file;
};

struct SHostTgaEncodeState
{
    std::int32_t executive_slot;
    CAssetId source;
    CAssetId request;
};

struct SHostTgaFileSaveState
{
    std::int32_t executive_slot;
    CAssetId encoded_file;
    CAssetId request;
};

}   //  namespace host

#endif  //  HOST_LOCAL_TYPES_HPP_INCLUDED
