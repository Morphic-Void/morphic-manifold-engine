
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    module_binding_context.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Common module-owned binding state and installation machinery.

#pragma once

#ifndef MODULE_BINDING_CONTEXT_HPP_INCLUDED
#define MODULE_BINDING_CONTEXT_HPP_INCLUDED

#include "modules/module_binding.hpp"

namespace modules
{

struct SModuleBindingConfig
{
    SAdvertisedModuleIdentity advertised_identity{};
    module_ids::id_type compatible_advertised_host_id{};
    std::uint32_t minimum_host_major{ 0u };
    std::uint32_t maximum_host_major{ 0u };
    FQueryFunction query_function{ nullptr };
};

class CModuleBindingContext
{
public:
    explicit CModuleBindingContext(const SModuleBindingConfig& config) noexcept;
    ~CModuleBindingContext() noexcept = default;

    CModuleBindingContext(const CModuleBindingContext&) = delete;
    CModuleBindingContext& operator=(const CModuleBindingContext&) = delete;
    CModuleBindingContext(CModuleBindingContext&&) = delete;
    CModuleBindingContext& operator=(CModuleBindingContext&&) = delete;

    EBindingResult bootstrap(SBootstrapFunctions* functions) noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] bool is_thread_context_ready(const void* provisioning) const noexcept;

private:
    static EBindingResult MV_STD_ABI_CALL query_advertised_module_identity(SAdvertisedModuleIdentity* identity) noexcept;
    static EBindingResult MV_STD_ABI_CALL install_advertised_host_identity(const SAdvertisedHostIdentity* identity) noexcept;
    static EBindingResult MV_STD_ABI_CALL populate_core_functions(
        std::uint32_t functional_major, SCoreFunctions* functions) noexcept;

    static EBindingResult MV_STD_ABI_CALL install_ambient_module_id(module_ids::id_type module_id) noexcept;
    static EBindingResult MV_STD_ABI_CALL install_module_memory_context(memory::CMemoryContext* context) noexcept;
    static EBindingResult MV_STD_ABI_CALL install_debug_service(debug_system::CDebugServiceState* service) noexcept;
    static EBindingResult MV_STD_ABI_CALL install_ambient_thread_id(thread_ids::id_type thread_id) noexcept;
    static EBindingResult MV_STD_ABI_CALL install_thread_memory_context(memory::CMemoryContext* context) noexcept;
    static EBindingResult MV_STD_ABI_CALL install_thread_provisioning(void* provisioning) noexcept;
    static EBindingResult MV_STD_ABI_CALL query_function(
        type_ids::id_type function_type,
        std::uint32_t functional_major,
        FModuleFunction* function) noexcept;

    SModuleBindingConfig m_config;
    SAdvertisedHostIdentity m_advertised_host_identity;
    bool m_advertised_host_identity_installed{ false };
    bool m_ambient_module_id_installed{ false };
    bool m_module_memory_context_installed{ false };
    bool m_debug_service_installed{ false };
};

[[nodiscard]] bool is_thread_context_ready(const void* provisioning) noexcept;

}   //  namespace modules

#endif  //  #ifndef MODULE_BINDING_CONTEXT_HPP_INCLUDED
