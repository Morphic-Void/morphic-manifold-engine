
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    host_erased_owner_operations.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    14 Aug 26
//
//  Host-local CErasedOwner operation table.

#include <array>

#include "host/module/types/host_local_type_registry.hpp"
#include "system/erased_owner.hpp"

namespace host
{

namespace internal
{

constexpr std::array<erased_owner_operations::SRegistration, host_local_type_ids::k_count> make_local_erased_owner_operations() noexcept
{
    std::array<erased_owner_operations::SRegistration, host_local_type_ids::k_count> result{};

#define MV_ERASED_OWNER_PAYLOAD(type) \
    result[local_type_ids::decode_index(local_type_ids::decode_id(k_local_type_id_v<type>))] = \
        erased_owner_operations::SRegistration{ k_type_id_v<type>, erased_owner_operations::TDefaultOperationsFactory<type>::make() };
#define MV_ERASED_OWNER_PAYLOAD_WITH_STORAGE(type, member) \
    result[local_type_ids::decode_index(local_type_ids::decode_id(k_local_type_id_v<type>))] = \
        erased_owner_operations::SRegistration{ k_type_id_v<type>, erased_owner_operations::TNestedOperationsFactory<type, &type::member>::make() };
#include "host/module/types/host_erased_owner_payloads.def"
#undef MV_ERASED_OWNER_PAYLOAD_WITH_STORAGE
#undef MV_ERASED_OWNER_PAYLOAD

    return result;
}

constexpr auto s_local_erased_owner_operations = make_local_erased_owner_operations();
constexpr erased_owner_operations::SCategoryView s_local_erased_owner_operations_view{
    s_local_erased_owner_operations.data(), static_cast<std::uint32_t>(s_local_erased_owner_operations.size()) };

}   //  namespace internal

const erased_owner_operations::SCategoryView& local_erased_owner_operations_view() noexcept
{
    return internal::s_local_erased_owner_operations_view;
}

}   //  namespace host
