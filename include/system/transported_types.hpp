
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   transported_types.hpp
//  Author: Ritchie Brannan
//  Date:   26 Jul 26
//
//  Built-in system payload declarations and C++ type registrations.

#pragma once

#ifndef TRANSPORTED_TYPES_HPP_INCLUDED
#define TRANSPORTED_TYPES_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::int32_t

#include "assets/asset_repository.hpp"
#include "containers/ByteBuffers.hpp"
#include "containers/StringBuffers.hpp"
#include "image/codec/tga.hpp"
#include "system/system_type_registration.hpp"

struct UnrecognisedMsg { type_ids::id_type msg_id; };

struct FileLoadRequest { const char* file; };
struct FileSaveRequest { const char* file; CByteConstView view; };
struct TgaLoadRequest { const char* file; bool vflip; };
struct TgaSaveRequest
{
    const char* file;
    CAssetId source;
    image::codec::tga::EncodeOptions options;
};
struct TgaEncodeRequest
{
    CByteRectConstView view;
    image::codec::tga::EncodeOptions options;
};
struct TgaDecodeRequest { CByteConstView view; bool vflip; };

struct FileLoadResult { CByteConstView* view; };
struct FileSaveResult { bool success; };
struct TgaLoadResult { CAssetId asset; image::codec::tga::decoded_image_desc desc; bool success; };
struct TgaSaveResult { bool success; };

struct OwningFileLoadResult { std::int32_t async_slot; CByteBuffer buffer; };
struct OwningTgaEncodeResult { std::int32_t async_slot; CByteBuffer buffer; };
struct OwningTgaDecodeResult { std::int32_t async_slot; CByteRectBuffer buffer; image::codec::tga::decoded_image_desc desc; };

MV_REGISTER_SYSTEM_TYPE(CByteBuffer, type_ids::byte_buffer);
MV_REGISTER_SYSTEM_TYPE(CByteRectBuffer, type_ids::byte_rect_buffer);
MV_REGISTER_SYSTEM_TYPE(CSimpleString, type_ids::simple_string);
MV_REGISTER_SYSTEM_TYPE(CStringBuffer, type_ids::string_buffer);
MV_REGISTER_SYSTEM_TYPE(CStableStrings, type_ids::stable_strings);

MV_REGISTER_SYSTEM_TYPE(UnrecognisedMsg, type_ids::unrecognised_msg);

MV_REGISTER_SYSTEM_TYPE(FileLoadRequest, type_ids::file_load_request);
MV_REGISTER_SYSTEM_TYPE(FileSaveRequest, type_ids::file_save_request);
MV_REGISTER_SYSTEM_TYPE(TgaLoadRequest, type_ids::tga_load_request);
MV_REGISTER_SYSTEM_TYPE(TgaSaveRequest, type_ids::tga_save_request);
MV_REGISTER_SYSTEM_TYPE(TgaEncodeRequest, type_ids::tga_encode_request);
MV_REGISTER_SYSTEM_TYPE(TgaDecodeRequest, type_ids::tga_decode_request);

MV_REGISTER_SYSTEM_TYPE(FileLoadResult, type_ids::file_load_result);
MV_REGISTER_SYSTEM_TYPE(FileSaveResult, type_ids::file_save_result);
MV_REGISTER_SYSTEM_TYPE(TgaLoadResult, type_ids::tga_load_result);
MV_REGISTER_SYSTEM_TYPE(TgaSaveResult, type_ids::tga_save_result);

MV_REGISTER_SYSTEM_TYPE(OwningFileLoadResult, type_ids::owning_file_load_result);
MV_REGISTER_ERASED_OWNER_PAYLOAD(OwningFileLoadResult);

MV_REGISTER_SYSTEM_TYPE(OwningTgaEncodeResult, type_ids::owning_tga_encode_result);
MV_REGISTER_ERASED_OWNER_PAYLOAD(OwningTgaEncodeResult);

MV_REGISTER_SYSTEM_TYPE(OwningTgaDecodeResult, type_ids::owning_tga_decode_result);
MV_REGISTER_ERASED_OWNER_PAYLOAD(OwningTgaDecodeResult);

#endif  //  #ifndef TRANSPORTED_TYPES_HPP_INCLUDED
