
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    bound_module.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Host-owned validated module binding and native module lifetime.

#include "host/bound_module.hpp"

#include "debug/macros.hpp"

namespace host
{

CBoundModule::~CBoundModule() noexcept
{
    unbind();
}

modules::EBindingResult CBoundModule::populate_core_functions(const std::uint32_t functional_major, modules::SCoreFunctions& functions) const noexcept
{
    functions = {};
    if (m_bootstrap.populate_core_functions == nullptr)
    {
        return modules::EBindingResult::invalid_argument;
    }

    return m_bootstrap.populate_core_functions(functional_major, &functions);
}

bool CBoundModule::bind(
    const platform::path::NativePath& path,
    const module_ids::id_type expected_advertised_module_id,
    const modules::SAdvertisedIdentity& host_identity) noexcept
{
    if (m_native_module.is_bound() ||
        !module_ids::is_valid_id(expected_advertised_module_id) ||
        !modules::is_valid_advertised_identity(host_identity) ||
        !m_native_module.bind(path))
    {
        return false;
    }

    const platform::module::FModuleFunction symbol = m_native_module.find_function(modules::k_bootstrap_symbol_name);
    const modules::FBootstrap bootstrap = reinterpret_cast<modules::FBootstrap>(symbol);
    modules::SBootstrapFunctions bootstrap_functions;
    if ((bootstrap == nullptr) ||
        (bootstrap(&bootstrap_functions) != modules::EBindingResult::success) ||
        (bootstrap_functions.query_advertised_identity == nullptr) ||
        (bootstrap_functions.install_peer_identity == nullptr) ||
        (bootstrap_functions.populate_core_functions == nullptr))
    {
        unbind();
        return false;
    }

    modules::SAdvertisedIdentity module_identity;
    if ((bootstrap_functions.query_advertised_identity(&module_identity) != modules::EBindingResult::success) ||
        !modules::is_valid_advertised_identity(module_identity) ||
        (module_identity.advertised_module_id != expected_advertised_module_id) ||
        !modules::functional_ranges_overlap(host_identity, module_identity) ||
        (bootstrap_functions.install_peer_identity(&host_identity) != modules::EBindingResult::success))
    {
        unbind();
        return false;
    }

    m_bootstrap = bootstrap_functions;
    m_negotiated_functional_major = modules::highest_common_functional_major(host_identity, module_identity);
    modules::SCoreFunctions core_functions;
    if ((populate_core_functions(m_negotiated_functional_major, core_functions) != modules::EBindingResult::success) ||
        !core_functions.is_complete())
    {
        unbind();
        return false;
    }

    m_core = core_functions;
    m_advertised_host_identity = host_identity;
    m_advertised_module_identity = module_identity;
    return true;
}

bool CBoundModule::install(
    const system_id_registry::SSystemRegistryView& system_registry,
    const module_ids::id_type ambient_module_id,
    memory::CMemoryContext* const module_memory_context,
    debug_system::CDebugServiceState* const debug_service) noexcept
{
    if (!m_native_module.is_bound() || m_installed ||
        !module_ids::is_valid_id(ambient_module_id) ||
        (m_core.install_system_registry_view(&system_registry) != modules::EBindingResult::success) ||
        (m_core.install_ambient_module_id(ambient_module_id) != modules::EBindingResult::success) ||
        (m_core.install_module_memory_context(module_memory_context) != modules::EBindingResult::success) ||
        (m_core.install_debug_service(debug_service) != modules::EBindingResult::success))
    {
        return false;
    }

    m_installed = true;
    return true;
}

bool CBoundModule::query_function(
    const system_type_id function_type,
    modules::FModuleFunction& function) const noexcept
{
    function = nullptr;
    if (!m_installed)
    {
        return false;
    }

    const modules::EBindingResult result = m_core.query_function(function_type, m_negotiated_functional_major, &function);
    return (result == modules::EBindingResult::success) && (function != nullptr);
}

bool MV_STD_ABI_CALL CBoundModule::prepare_thread(
    void* const context,
    const thread_ids::id_type thread_id,
    void* const thread_resources) noexcept
{
    CBoundModule* const module = static_cast<CBoundModule*>(context);
    return (module != nullptr) && module->prepare_thread(thread_id, thread_resources);
}

bool CBoundModule::prepare_thread(
    const thread_ids::id_type thread_id,
    void* const thread_resources) noexcept
{
    return m_installed &&
        (m_core.install_ambient_thread_id(thread_id) == modules::EBindingResult::success) &&
        (m_core.install_thread_provisioning(thread_resources) == modules::EBindingResult::success) &&
        (m_core.install_thread_memory_context(nullptr) == modules::EBindingResult::success);
}

void CBoundModule::unbind() noexcept
{
    m_installed = false;
    m_negotiated_functional_major = 0u;
    m_advertised_host_identity = {};
    m_advertised_module_identity = {};
    m_core = {};
    m_bootstrap = {};
    (void)m_native_module.unbind();
}

}   //  namespace host
