
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    executive_local_type_registry.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    12 Aug 26

#pragma once

#ifndef EXECUTIVE_LOCAL_TYPE_REGISTRY_HPP_INCLUDED
#define EXECUTIVE_LOCAL_TYPE_REGISTRY_HPP_INCLUDED

#include "system/local_type_registration.hpp"
#include "system/local_type_registry.hpp"
#include "system/erased_owner_operations.hpp"
#include "platform/platform_defines.hpp"

namespace executive
{
struct CExecutiveModuleTag {};
struct SExecutiveTgaLoadState;
struct SExecutiveTgaSaveState;
[[nodiscard]] const local_type_registry::SLocalTypeRegistryView& MV_STD_ABI_CALL local_type_registry_view() noexcept;
[[nodiscard]] const erased_owner_operations::SCategoryView& MV_STD_ABI_CALL local_erased_owner_operations_view() noexcept;
}

namespace executive_local_type_ids
{
#define MV_LOCAL_TYPE(name, cpp_type) name##_index_value,
enum : local_type_ids::index_type
{
#include "executive/module/types/executive_local_type_ids.def"
    k_count
};
#undef MV_LOCAL_TYPE

static_assert(k_count <= local_type_ids::ops::k_capacity);

#define MV_LOCAL_TYPE(name, cpp_type) \
    inline constexpr local_type_ids::index_type name##_index = \
        local_type_ids::ops::encode_index(name##_index_value); \
    inline constexpr local_type_id name = \
        local_type_ids::ops::encode_id(name##_index);
#include "executive/module/types/executive_local_type_ids.def"
#undef MV_LOCAL_TYPE
}

#define MV_LOCAL_TYPE(name, cpp_type) \
    MV_REGISTER_LOCAL_TYPE(cpp_type, executive_local_type_ids::name);
#include "executive/module/types/executive_local_type_ids.def"
#undef MV_LOCAL_TYPE

#endif  //  EXECUTIVE_LOCAL_TYPE_REGISTRY_HPP_INCLUDED
