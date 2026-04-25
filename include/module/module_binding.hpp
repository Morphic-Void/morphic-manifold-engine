
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   module_binding.hpp
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

#include "io/path/native_path.hpp"

namespace module_binding
{

struct CPlatformModule { void* native_handle = nullptr; };

const char* getModulesStdExt() noexcept;
CPlatformModule bindModule(const io::path::NativePath& path) noexcept;
void* findModuleSymbol(const CPlatformModule& module, const char* const symbol_name) noexcept;
bool unbindModule(CPlatformModule& module) noexcept;

}   //  namespace module_binding

#endif  //  #ifndef MODULE_BINDING_HPP_INCLUDED
