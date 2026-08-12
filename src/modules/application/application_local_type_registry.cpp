
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include "modules/application/application_local_type_registry.hpp"

namespace application
{

namespace
{
constexpr local_type_registry::SLocalTypeRegistration s_types[] =
{
#define MV_LOCAL_TYPE(name, cpp_type) \
    { application_local_type_ids::name, application_local_type_ids::name##_index, local_type_registry::make_name(#name) },
#include "modules/application/application_local_type_ids.def"
#undef MV_LOCAL_TYPE
};

constexpr local_type_registry::SLocalTypeRegistryView s_view{ s_types, static_cast<std::uint32_t>(application_local_type_ids::k_count) };
}

const local_type_registry::SLocalTypeRegistryView& MV_STD_ABI_CALL local_type_registry_view() noexcept
{
    return s_view;
}

}   //  namespace application
