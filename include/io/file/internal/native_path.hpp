//  File:   native_path.hpp
//  Author: Ritchie Brannan
//  Date:   1 Mar 26
//
//  Platform native path container and utils
//
//  Notes:
//  - Requires C++ 17 or greater
//  - No exceptions.

#pragma once

#ifndef NATIVE_PATH_HPP_INCLUDED
#define NATIVE_PATH_HPP_INCLUDED

#include "containers/TPodVector.hpp"

namespace io::file
{

#if defined(_WIN32) || defined(_WIN64)
#include <wchar.h>
using native_char = wchar_t;
#else
using native_char = char;
#endif

using NativePath = TPodVector<native_char>;
NativePath stdPath(const char* utf8_path) noexcept;         // UTF-8 -> native (and policy)
NativePath tmpPath(const NativePath& std_path) noexcept;   // append ".tmp"

}   //  namespace io::file

#endif  //  #ifndef NATIVE_PATH_HPP_INCLUDED
