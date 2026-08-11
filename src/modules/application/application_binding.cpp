
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    application_binding.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Application module identity, exported functions, and bootstrap entry point.

#include "modules/application/application_binding.hpp"

#include "modules/application/application_thread.hpp"
#include "modules/module_binding_context.hpp"

namespace application::module_binding
{

static modules::EBindingResult MV_STD_ABI_CALL query_function(
    const type_ids::id_type function_type,
    const std::uint32_t,
    modules::FModuleFunction* const function) noexcept
{
    if (function == nullptr)
    {
        return modules::EBindingResult::invalid_argument;
    }
    *function = nullptr;

    if (function_type != type_ids::application_thread_function)
    {
        return modules::EBindingResult::unsupported_function;
    }

    *function = reinterpret_cast<modules::FModuleFunction>(application_thread_entry_point());
    return modules::EBindingResult::success;
}

constexpr modules::SModuleBindingConfig k_binding_config{
    { module_ids::application, { 1u, 1u }, 0u, 1u },
    module_ids::executable,
    1u,
    1u,
    &query_function
};

modules::CModuleBindingContext s_binding{ k_binding_config };

}   //  namespace application::module_binding

MV_MODULE_EXPORT modules::EBindingResult MV_STD_ABI_CALL morphic_module_bootstrap(
    modules::SBootstrapFunctions* const functions) noexcept
{
    return application::module_binding::s_binding.bootstrap(functions);
}
