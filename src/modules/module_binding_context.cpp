
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    module_binding_context.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Common module-owned binding state and installation machinery.

#include "modules/module_binding_context.hpp"

#include "debug/service.hpp"
#include "memory/memory_context.hpp"
#include "system/system_context.hpp"

namespace modules
{

namespace
{

CModuleBindingContext* s_active_binding{ nullptr };
thread_local void* t_thread_provisioning{ nullptr };

}   //  namespace

CModuleBindingContext::CModuleBindingContext(const SModuleBindingConfig& config) noexcept
    : m_config{ config }
{
}

bool is_thread_context_ready(const void* const provisioning) noexcept
{
    return (s_active_binding != nullptr) &&
        s_active_binding->is_thread_context_ready(provisioning);
}

bool CModuleBindingContext::is_ready() const noexcept
{
    return m_advertised_host_identity_installed &&
        m_ambient_module_id_installed &&
        m_module_memory_context_installed &&
        m_debug_service_installed;
}

bool CModuleBindingContext::is_thread_context_ready(const void* const provisioning) const noexcept
{
    return is_ready() &&
        (provisioning != nullptr) &&
        (t_thread_provisioning == provisioning) &&
        module_ids::is_valid_id(system_context::get_ambient_module_id()) &&
        thread_ids::is_valid_id(system_context::get_ambient_thread_id()) &&
        (memory::get_ambient_memory_context() != nullptr) &&
        (debug_system::get_service() != nullptr);
}

EBindingResult CModuleBindingContext::bootstrap(SBootstrapFunctions* const functions) noexcept
{
    if ((functions == nullptr) || (m_config.query_function == nullptr) ||
        !module_ids::is_valid_id(m_config.advertised_identity.advertised_module_id) ||
        !module_ids::is_valid_id(m_config.compatible_advertised_host_id) ||
        (m_config.advertised_identity.minimum_functional_major >
            m_config.advertised_identity.maximum_functional_major) ||
        (m_config.minimum_host_major > m_config.maximum_host_major))
    {
        return EBindingResult::invalid_argument;
    }

    if ((s_active_binding != nullptr) && (s_active_binding != this))
    {
        return EBindingResult::already_installed;
    }
    s_active_binding = this;

    SBootstrapFunctions populated;
    populated.query_advertised_module_identity = &query_advertised_module_identity;
    populated.install_advertised_host_identity = &install_advertised_host_identity;
    populated.populate_core_functions = &populate_core_functions;
    *functions = populated;
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::query_advertised_module_identity(
    SAdvertisedModuleIdentity* const identity) noexcept
{
    if ((identity == nullptr) || (s_active_binding == nullptr))
    {
        return EBindingResult::invalid_argument;
    }

    *identity = s_active_binding->m_config.advertised_identity;
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::install_advertised_host_identity(
    const SAdvertisedHostIdentity* const identity) noexcept
{
    if ((identity == nullptr) || (s_active_binding == nullptr))
    {
        return EBindingResult::invalid_argument;
    }

    CModuleBindingContext& binding = *s_active_binding;
    if ((identity->advertised_module_id != binding.m_config.compatible_advertised_host_id) ||
        (identity->version.major < binding.m_config.minimum_host_major) ||
        (identity->version.major > binding.m_config.maximum_host_major))
    {
        return EBindingResult::incompatible_host;
    }
    if (binding.m_advertised_host_identity_installed)
    {
        return EBindingResult::already_installed;
    }

    binding.m_advertised_host_identity = *identity;
    binding.m_advertised_host_identity_installed = true;
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::install_ambient_module_id(
    const module_ids::id_type module_id) noexcept
{
    if ((s_active_binding == nullptr) || !module_ids::is_valid_id(module_id))
    {
        return EBindingResult::invalid_argument;
    }

    (void)system_context::set_ambient_module_id(module_id);
    s_active_binding->m_ambient_module_id_installed = true;
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::install_module_memory_context(
    memory::CMemoryContext* const context) noexcept
{
    if ((s_active_binding == nullptr) || (context == nullptr) || !context->is_usable())
    {
        return EBindingResult::invalid_argument;
    }

    (void)memory::set_module_memory_context(context);
    s_active_binding->m_module_memory_context_installed = true;
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::install_debug_service(
    debug_system::CDebugServiceState* const service) noexcept
{
    if ((s_active_binding == nullptr) || (service == nullptr))
    {
        return EBindingResult::invalid_argument;
    }
    if (s_active_binding->m_debug_service_installed)
    {
        return EBindingResult::already_installed;
    }
    if (!debug_system::install_service(service))
    {
        return EBindingResult::already_installed;
    }

    s_active_binding->m_debug_service_installed = true;
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::install_ambient_thread_id(
    const thread_ids::id_type thread_id) noexcept
{
    if ((s_active_binding == nullptr) || !s_active_binding->is_ready() ||
        !thread_ids::is_valid_id(thread_id))
    {
        return EBindingResult::invalid_argument;
    }

    (void)system_context::set_ambient_thread_id(thread_id);
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::install_thread_memory_context(
    memory::CMemoryContext* const context) noexcept
{
    if ((s_active_binding == nullptr) || !s_active_binding->is_ready() ||
        ((context != nullptr) && !context->is_usable()))
    {
        return EBindingResult::invalid_argument;
    }

    (void)memory::set_thread_memory_context(context);
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::install_thread_provisioning(
    void* const provisioning) noexcept
{
    if ((s_active_binding == nullptr) || !s_active_binding->is_ready() ||
        (provisioning == nullptr))
    {
        return EBindingResult::invalid_argument;
    }

    t_thread_provisioning = provisioning;
    return EBindingResult::success;
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::query_function(
    const type_ids::id_type function_type,
    const std::uint32_t functional_major,
    FModuleFunction* const function) noexcept
{
    if ((s_active_binding == nullptr) || !s_active_binding->is_ready() || (function == nullptr))
    {
        return EBindingResult::invalid_argument;
    }

    *function = nullptr;
    const SAdvertisedModuleIdentity& identity = s_active_binding->m_config.advertised_identity;
    if ((functional_major < identity.minimum_functional_major) ||
        (functional_major > identity.maximum_functional_major))
    {
        return EBindingResult::unsupported_version;
    }
    return s_active_binding->m_config.query_function(function_type, functional_major, function);
}

EBindingResult MV_STD_ABI_CALL CModuleBindingContext::populate_core_functions(
    const std::uint32_t functional_major,
    SCoreFunctions* const functions) noexcept
{
    if ((functions == nullptr) || (s_active_binding == nullptr))
    {
        return EBindingResult::invalid_argument;
    }
    *functions = {};

    const SAdvertisedModuleIdentity& identity = s_active_binding->m_config.advertised_identity;
    if ((functional_major < identity.minimum_functional_major) ||
        (functional_major > identity.maximum_functional_major))
    {
        return EBindingResult::unsupported_version;
    }
    if (!s_active_binding->m_advertised_host_identity_installed)
    {
        return EBindingResult::incompatible_host;
    }

    SCoreFunctions populated;
    populated.install_ambient_module_id = &install_ambient_module_id;
    populated.install_module_memory_context = &install_module_memory_context;
    populated.install_debug_service = &install_debug_service;
    populated.install_ambient_thread_id = &install_ambient_thread_id;
    populated.install_thread_memory_context = &install_thread_memory_context;
    populated.install_thread_provisioning = &install_thread_provisioning;
    populated.query_function = &query_function;
    *functions = populated;
    return EBindingResult::success;
}

}   //  namespace modules
