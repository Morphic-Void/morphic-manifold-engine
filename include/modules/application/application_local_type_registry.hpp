
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#pragma once

#ifndef APPLICATION_LOCAL_TYPE_REGISTRY_HPP_INCLUDED
#define APPLICATION_LOCAL_TYPE_REGISTRY_HPP_INCLUDED

#include "system/local_type_registration.hpp"
#include "system/local_type_registry.hpp"
#include "platform/platform_defines.hpp"

namespace application
{
struct CApplicationModuleTag {};
struct SApplicationTgaLoadState;
struct SApplicationTgaSaveState;
[[nodiscard]] const local_type_registry::SLocalTypeRegistryView& MV_STD_ABI_CALL local_type_registry_view() noexcept;
}

namespace application_local_type_ids
{
#define MV_LOCAL_TYPE(name, cpp_type) name##_index_value,
enum : local_type_ids::index_type
{
#include "modules/application/application_local_type_ids.def"
    k_count
};
#undef MV_LOCAL_TYPE

static_assert(k_count <= local_type_ids::k_capacity);

#define MV_LOCAL_TYPE(name, cpp_type) \
    inline constexpr local_type_ids::index_type name##_index = \
        local_type_ids::encode_index(name##_index_value); \
    inline constexpr local_type_id name = \
        local_type_ids::encode_id(name##_index);
#include "modules/application/application_local_type_ids.def"
#undef MV_LOCAL_TYPE
}

#define MV_LOCAL_TYPE(name, cpp_type) \
    MV_REGISTER_LOCAL_TYPE(cpp_type, application_local_type_ids::name);
#include "modules/application/application_local_type_ids.def"
#undef MV_LOCAL_TYPE

#endif  //  APPLICATION_LOCAL_TYPE_REGISTRY_HPP_INCLUDED
