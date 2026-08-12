
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include "host/host_local_type_registry.hpp"

namespace host
{

namespace
{
constexpr local_type_registry::SLocalTypeRegistration s_types[] =
{
#define MV_LOCAL_TYPE(name, cpp_type) \
    { host_local_type_ids::name, host_local_type_ids::name##_index, local_type_registry::make_name(#name) },
#include "host/host_local_type_ids.def"
#undef MV_LOCAL_TYPE
};

constexpr local_type_registry::SLocalTypeRegistryView s_view{ s_types, static_cast<std::uint32_t>(host_local_type_ids::k_count) };
}

const local_type_registry::SLocalTypeRegistryView& local_type_registry_view() noexcept
{
    return s_view;
}

}   //  namespace host
