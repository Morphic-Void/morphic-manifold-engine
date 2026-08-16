
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    host_local_types.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    14 Aug 26
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

struct SHostTgaFileLoadState
{
    std::int32_t executive_slot;
    const char* file;
    bool vflip;
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
    const char* file;
    image::codec::tga::EncodeOptions options;
};

struct SHostTgaFileSaveState
{
    std::int32_t executive_slot;
    CAssetId encoded_file;
};

}   //  namespace host

#endif  //  HOST_LOCAL_TYPES_HPP_INCLUDED
