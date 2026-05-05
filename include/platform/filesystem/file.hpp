
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   file.hpp
//  Author: Ritchie Brannan
//  Date:   1 Mar 26
//
//  Basic load and save utility functions (file <=> memory blob)
//
//  Multi-threaded usage assumes that multiple threads will not be saving
//  files with the same name.

#pragma once

#ifndef FILE_HPP_INCLUDED
#define FILE_HPP_INCLUDED

#include <stddef.h>
#include "containers/ByteBuffers.hpp"

namespace platform::filesystem
{

//  utf8_path is expected to be UTF-8 encoded.

//  Empty files are treated as failure.
//  On failure, the buffer is empty.
//  On success, the buffer size includes pad bytes, which are zero-filled.
CByteBuffer loadFile(const char* const utf8_path, const size_t pad = 0) noexcept;

//  Will not write empty files.
//  Semi-atomic file writing.
//  On failure, best effort is made clean up partial writes.
bool saveFile(const char* const utf8_path, const CByteView& view) noexcept;

}   //  namespace platform::filesystem

#endif  //  #ifndef FILE_HPP_INCLUDED
