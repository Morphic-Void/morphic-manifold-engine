
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    application_local_type_registry.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    12 Aug 26

#include "modules/application/application_local_type_registry.hpp"

namespace application
{

namespace local_types
{
constexpr local_type_registry::SLocalTypeRegistration s_types[] =
{
#define MV_LOCAL_TYPE(name, cpp_type) \
    { application_local_type_ids::name, application_local_type_ids::name##_index, local_type_registry::make_name(#name) },
#include "modules/application/application_local_type_ids.def"
#undef MV_LOCAL_TYPE
};

constexpr local_type_registry::SLocalTypeRegistryView s_view{ s_types, static_cast<std::uint32_t>(application_local_type_ids::k_count) };
}   //  namespace local_types

const local_type_registry::SLocalTypeRegistryView& MV_STD_ABI_CALL local_type_registry_view() noexcept
{
    return local_types::s_view;
}

}   //  namespace application
