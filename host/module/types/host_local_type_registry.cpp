
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    host_local_type_registry.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    12 Aug 26

#include "host/module/types/host_local_type_registry.hpp"

namespace host
{

namespace local_types
{
constexpr local_type_registry::SLocalTypeRegistration s_types[] =
{
#define MV_LOCAL_TYPE(name, cpp_type) \
    { host_local_type_ids::name, host_local_type_ids::name##_index, local_type_registry::make_name(#name) },
#include "host/module/types/host_local_type_ids.def"
#undef MV_LOCAL_TYPE
};

constexpr local_type_registry::SLocalTypeRegistryView s_view{ s_types, static_cast<std::uint32_t>(host_local_type_ids::k_count) };
}   //  namespace local_types

const local_type_registry::SLocalTypeRegistryView& local_type_registry_view() noexcept
{
    return local_types::s_view;
}

}   //  namespace host
