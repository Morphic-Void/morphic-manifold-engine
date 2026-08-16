
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    executive_binding.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Executive module identity, exported functions, and bootstrap entry point.

#include "executive/module/binding/executive_binding.hpp"

#include "executive/module/types/executive_local_type_registry.hpp"
#include "executive/runtime/executive_thread.hpp"
#include "module/module_binding_context.hpp"

namespace executive::module_binding
{

static modules::EBindingResult MV_STD_ABI_CALL query_function(
    const system_type_id function_type,
    const std::uint32_t,
    modules::FModuleFunction* const function) noexcept
{
    if (function == nullptr)
    {
        return modules::EBindingResult::invalid_argument;
    }
    *function = nullptr;

    if (function_type != system_type_ids::executive_thread_function)
    {
        return modules::EBindingResult::unsupported_function;
    }

    *function = reinterpret_cast<modules::FModuleFunction>(executive_thread_entry_point());
    return modules::EBindingResult::success;
}

constexpr modules::SModuleBindingConfig k_binding_config{
    { module_ids::executive, { modules::k_binding_abi_major, 0u },
        modules::k_binding_abi_major, modules::k_binding_abi_major },
    module_ids::executable,
    modules::k_binding_abi_major,
    modules::k_binding_abi_major,
    &query_function,
    &executive::local_type_registry_view,
    &executive::local_erased_owner_operations_view
};

modules::CModuleBindingContext s_binding{ k_binding_config };

}   //  namespace executive::module_binding

MV_MODULE_EXPORT modules::EBindingResult MV_STD_ABI_CALL morphic_module_bootstrap_v3(
    modules::SBootstrapFunctions* const functions) noexcept
{
    return executive::module_binding::s_binding.bootstrap(functions);
}
