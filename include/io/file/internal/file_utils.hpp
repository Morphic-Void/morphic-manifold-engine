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

#include "native_path.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#else
//  default, assumes _POSIX_VERSION or __APPLE__ or__linux__ or similar
    #include <sys/types.h>
#endif

namespace io::file
{

enum class OpenMode : std::uint8_t { BinaryRead = 0u, BinaryWrite, BinaryAppend, TextWrite, TextAppend };

FILE* openFile(const NativePath& file_path, const OpenMode mode = OpenMode::BinaryRead) noexcept;
void removeFile(const NativePath& file_path) noexcept;
bool renameFile(const NativePath& src_path, const NativePath& dst_path) noexcept;
bool flushToDisk(std::FILE* const handle) noexcept;
std::size_t getFileSize(std::FILE* const handle, const std::size_t pad = 0) noexcept;

}   //  namespace io::file

#endif  //  #ifndef FILE_UTILS_HPP_INCLUDED
