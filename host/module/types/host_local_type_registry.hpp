
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    host_local_type_registry.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    12 Aug 26

#pragma once

#ifndef HOST_LOCAL_TYPE_REGISTRY_HPP_INCLUDED
#define HOST_LOCAL_TYPE_REGISTRY_HPP_INCLUDED

#include "host/module/types/host_local_types.hpp"
#include "system/erased_owner_operations.hpp"
#include "system/erased_owner_registration.hpp"
#include "system/local_type_registration.hpp"
#include "system/local_type_registry.hpp"

namespace host
{
class CHost;
[[nodiscard]] const local_type_registry::SLocalTypeRegistryView& local_type_registry_view() noexcept;
[[nodiscard]] const erased_owner_operations::SCategoryView& local_erased_owner_operations_view() noexcept;
}

namespace host_local_type_ids
{
#define MV_LOCAL_TYPE(name, cpp_type) name##_index_value,
enum : local_type_ids::index_type
{
#include "host/module/types/host_local_type_ids.def"
    k_count
};
#undef MV_LOCAL_TYPE

static_assert(k_count <= local_type_ids::k_capacity);

#define MV_LOCAL_TYPE(name, cpp_type) \
    inline constexpr local_type_ids::index_type name##_index = \
        local_type_ids::encode_index(name##_index_value); \
    inline constexpr local_type_id name = \
        local_type_ids::encode_id(name##_index);
#include "host/module/types/host_local_type_ids.def"
#undef MV_LOCAL_TYPE
}

#define MV_LOCAL_TYPE(name, cpp_type) \
    MV_REGISTER_LOCAL_TYPE(cpp_type, host_local_type_ids::name);
#include "host/module/types/host_local_type_ids.def"
#undef MV_LOCAL_TYPE

#define MV_ERASED_OWNER_PAYLOAD(type) MV_REGISTER_ERASED_OWNER_PAYLOAD(type);
#define MV_ERASED_OWNER_PAYLOAD_WITH_STORAGE(type, member) \
    MV_REGISTER_ERASED_OWNER_PAYLOAD(type);
#include "host/module/types/host_erased_owner_payloads.def"
#undef MV_ERASED_OWNER_PAYLOAD_WITH_STORAGE
#undef MV_ERASED_OWNER_PAYLOAD

#endif  //  HOST_LOCAL_TYPE_REGISTRY_HPP_INCLUDED
