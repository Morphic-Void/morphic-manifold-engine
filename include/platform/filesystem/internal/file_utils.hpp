
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   file_utils.hpp
//  Author: Ritchie Brannan
//  Date:   1 Mar 26
//
//  Platform native file handling utilities
//
//  Notes:
//  - Requires C++ 17 or greater
//  - No exceptions.

#pragma once

#ifndef FILE_UTILS_HPP_INCLUDED
#define FILE_UTILS_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include "platform/path/native_path.hpp"

namespace platform::filesystem
{

enum class OpenMode : std::uint8_t { BinaryRead = 0u, BinaryWrite, BinaryAppend, TextWrite, TextAppend };

FILE* openFile(const path::NativePath& file_path, const OpenMode mode = OpenMode::BinaryRead) noexcept;
void removeFile(const path::NativePath& file_path) noexcept;
bool renameFile(const path::NativePath& src_path, const path::NativePath& dst_path) noexcept;
bool flushToDisk(std::FILE* const handle) noexcept;
std::size_t getFileSize(std::FILE* const handle, const std::size_t pad = 0) noexcept;

}   //  namespace platform::filesystem

#endif  //  #ifndef FILE_UTILS_HPP_INCLUDED
