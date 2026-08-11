
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    module_binding.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Shared host/module binding ABI.

#pragma once

#ifndef MODULE_BINDING_ABI_HPP_INCLUDED
#define MODULE_BINDING_ABI_HPP_INCLUDED

#include <cstdint>      //  std::uint32_t
#include <type_traits>  //  std::is_standard_layout_v, std::is_trivially_copyable_v

#include "platform/platform_defines.hpp"
#include "system/system_ids.hpp"

//==============================================================================
//  External declarations
//==============================================================================

namespace debug_system
{
class CDebugServiceState;
}

namespace memory
{
class CMemoryContext;
}

namespace modules
{

inline constexpr char k_bootstrap_symbol_name[]{ "morphic_module_bootstrap" };

struct SVersion
{
    std::uint32_t major{ 0u };
    std::uint32_t minor{ 0u };
};

struct SAdvertisedHostIdentity
{
    module_ids::id_type advertised_module_id{};
    SVersion version{};
};

struct SAdvertisedModuleIdentity
{
    module_ids::id_type advertised_module_id{};
    SVersion version{};
    std::uint32_t minimum_functional_major{ 0u };
    std::uint32_t maximum_functional_major{ 0u };
};

enum class EBindingResult : std::uint32_t
{
    success = 0u,
    invalid_argument,
    incompatible_host,
    unsupported_version,
    unsupported_function,
    already_installed
};

using FModuleFunction = void(MV_STD_ABI_CALL*)() noexcept;

using FQueryAdvertisedModuleIdentity = EBindingResult(MV_STD_ABI_CALL*)(SAdvertisedModuleIdentity* identity) noexcept;
using FInstallAdvertisedHostIdentity = EBindingResult(MV_STD_ABI_CALL*)(const SAdvertisedHostIdentity* identity) noexcept;

using FInstallAmbientModuleId = EBindingResult(MV_STD_ABI_CALL*)(module_ids::id_type module_id) noexcept;
using FInstallModuleMemoryContext = EBindingResult(MV_STD_ABI_CALL*)(memory::CMemoryContext* context) noexcept;
using FInstallDebugService = EBindingResult(MV_STD_ABI_CALL*)(debug_system::CDebugServiceState* service) noexcept;
using FInstallAmbientThreadId = EBindingResult(MV_STD_ABI_CALL*)(thread_ids::id_type thread_id) noexcept;
using FInstallThreadMemoryContext = EBindingResult(MV_STD_ABI_CALL*)(memory::CMemoryContext* context) noexcept;
using FInstallThreadProvisioning = EBindingResult(MV_STD_ABI_CALL*)(void* provisioning) noexcept;
using FQueryFunction = EBindingResult(MV_STD_ABI_CALL*)(type_ids::id_type function_type, std::uint32_t functional_major, FModuleFunction* function) noexcept;

struct SCoreFunctions
{
    FInstallAmbientModuleId install_ambient_module_id{ nullptr };
    FInstallModuleMemoryContext install_module_memory_context{ nullptr };
    FInstallDebugService install_debug_service{ nullptr };
    FInstallAmbientThreadId install_ambient_thread_id{ nullptr };
    FInstallThreadMemoryContext install_thread_memory_context{ nullptr };
    FInstallThreadProvisioning install_thread_provisioning{ nullptr };
    FQueryFunction query_function{ nullptr };
};

using FPopulateCoreFunctions = EBindingResult(MV_STD_ABI_CALL*)(std::uint32_t functional_major, SCoreFunctions* functions) noexcept;

struct SBootstrapFunctions
{
    FQueryAdvertisedModuleIdentity query_advertised_module_identity{ nullptr };
    FInstallAdvertisedHostIdentity install_advertised_host_identity{ nullptr };
    FPopulateCoreFunctions populate_core_functions{ nullptr };
};

using FBootstrap = EBindingResult(MV_STD_ABI_CALL*)(SBootstrapFunctions* functions) noexcept;

static_assert(std::is_standard_layout_v<SVersion> && std::is_trivially_copyable_v<SVersion>);
static_assert(std::is_standard_layout_v<SAdvertisedHostIdentity> && std::is_trivially_copyable_v<SAdvertisedHostIdentity>);
static_assert(std::is_standard_layout_v<SAdvertisedModuleIdentity> && std::is_trivially_copyable_v<SAdvertisedModuleIdentity>);
static_assert(std::is_standard_layout_v<SCoreFunctions> && std::is_trivially_copyable_v<SCoreFunctions>);
static_assert(std::is_standard_layout_v<SBootstrapFunctions> && std::is_trivially_copyable_v<SBootstrapFunctions>);

}   //  namespace modules

#endif  //  #ifndef MODULE_BINDING_ABI_HPP_INCLUDED
