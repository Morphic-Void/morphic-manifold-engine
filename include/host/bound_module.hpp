
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    bound_module.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Host-owned validated module binding and native module lifetime.

#pragma once

#ifndef HOST_BOUND_MODULE_HPP_INCLUDED
#define HOST_BOUND_MODULE_HPP_INCLUDED

#include "modules/module_binding.hpp"
#include "platform/module/binding.hpp"

namespace host
{

class CBoundModule
{
public:
    CBoundModule() noexcept = default;
    CBoundModule(const CBoundModule&) = delete;
    CBoundModule& operator=(const CBoundModule&) = delete;
    CBoundModule(CBoundModule&&) = delete;
    CBoundModule& operator=(CBoundModule&&) = delete;
    ~CBoundModule() noexcept;

    [[nodiscard]] bool bind(
        const platform::path::NativePath& path,
        module_ids::id_type expected_advertised_module_id,
        const modules::SAdvertisedHostIdentity& advertised_host_identity,
        std::uint32_t functional_major) noexcept;
    [[nodiscard]] bool install(
        module_ids::id_type ambient_module_id,
        memory::CMemoryContext* module_memory_context,
        debug_system::CDebugServiceState* debug_service) noexcept;
    void unbind() noexcept;

    [[nodiscard]] bool populate_core_functions(
        std::uint32_t functional_major, modules::SCoreFunctions& functions) const noexcept;
    [[nodiscard]] bool query_function(
        type_ids::id_type function_type,
        std::uint32_t functional_major,
        modules::FModuleFunction& function) const noexcept;
    [[nodiscard]] const modules::SAdvertisedModuleIdentity& advertised_identity() const noexcept { return m_advertised_identity; }
    [[nodiscard]] bool is_ready() const noexcept { return m_installed; }

    static bool MV_STD_ABI_CALL prepare_thread(
        void* context, thread_ids::id_type thread_id, void* thread_resources) noexcept;

private:
    [[nodiscard]] static bool core_functions_are_complete(const modules::SCoreFunctions& functions) noexcept;
    [[nodiscard]] bool prepare_thread(
        thread_ids::id_type thread_id, void* thread_resources) noexcept;

    platform::module::CPlatformModule m_native_module;
    modules::SBootstrapFunctions m_bootstrap;
    modules::SCoreFunctions m_core;
    modules::SAdvertisedModuleIdentity m_advertised_identity;
    bool m_installed{ false };
};

}   //  namespace host

#endif  //  #ifndef HOST_BOUND_MODULE_HPP_INCLUDED
