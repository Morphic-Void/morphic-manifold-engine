
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   binding.hpp
//  Author: Ritchie Brannan
//  Date:   25 Apr 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Module (DLL/SO/DYLIB) binding (load/query/unload)
//
//  Note: getModulesStdExt() returns the extension without any preceding '.'

#pragma once

#ifndef MODULE_BINDING_HPP_INCLUDED
#define MODULE_BINDING_HPP_INCLUDED

#include "platform/path/native_path.hpp"

namespace platform::module
{

struct CPlatformModule { void* native_handle = nullptr; };

const char* getModulesStdExt() noexcept;
CPlatformModule bindModule(const platform::path::NativePath& path) noexcept;
void* findModuleSymbol(const CPlatformModule& module, const char* const symbol_name) noexcept;
bool unbindModule(CPlatformModule& module) noexcept;

}   //  namespace platform::module

#endif  //  #ifndef MODULE_BINDING_HPP_INCLUDED
