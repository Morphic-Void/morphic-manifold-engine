
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   binding.cpp
//  Author: Ritchie Brannan
//  Date:   25 Apr 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Module (DLL/SO/DYLIB) binding (load/query/unload)

#include "platform/module/binding.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace platform::module
{

const char* getModulesStdExt() noexcept
{
#if defined(_WIN32) || defined(_WIN64)
    static const char* k_ext = "dll";
#elif defined(__APPLE__)
    static const char* k_ext = "dylib";
#else   //  default, assumes _POSIX_VERSION or __linux__ or similar
    static const char* k_ext = "so";
#endif
    return k_ext;
}

CPlatformModule bindModule(const platform::path::NativePath& path) noexcept
{
    CPlatformModule module;

    if (!path.is_empty())
    {
#if defined(_WIN32) || defined(_WIN64)
        HMODULE handle = LoadLibraryW(path.data());
        if (handle != nullptr)
        {
            module.native_handle = reinterpret_cast<void*>(handle);
        }
#else
        void* handle = dlopen(path.data(), RTLD_NOW | RTLD_LOCAL);
        if (handle != nullptr)
        {
            module.native_handle = handle;
        }
#endif
    }

    return module;
}

void* findModuleSymbol(const CPlatformModule& module, const char* const symbol_name) noexcept
{
    void* symbol = nullptr;

    if ((module.native_handle != nullptr) && (symbol_name != nullptr) && (symbol_name[0] != 0))
    {
#if defined(_WIN32) || defined(_WIN64)
        HMODULE handle = reinterpret_cast<HMODULE>(module.native_handle);
        FARPROC proc = GetProcAddress(handle, symbol_name);
        if (proc != nullptr)
        {
            symbol = reinterpret_cast<void*>(proc);
        }
#else
        symbol = dlsym(module.native_handle, symbol_name);
#endif
    }

    return symbol;
}

bool unbindModule(CPlatformModule& module) noexcept
{
    bool success = true;

    if (module.native_handle != nullptr)
    {
#if defined(_WIN32) || defined(_WIN64)
        HMODULE handle = reinterpret_cast<HMODULE>(module.native_handle);
        success = (FreeLibrary(handle) != 0);
#else
        success = (dlclose(module.native_handle) == 0);
#endif
        if (success)
        {
            module.native_handle = nullptr;
        }
    }

    return success;
}

}   //  namespace platform::module
