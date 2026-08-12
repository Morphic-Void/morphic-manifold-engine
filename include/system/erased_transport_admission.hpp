//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  Destination-aware identity admission for concrete erased transports.

#pragma once

#ifndef ERASED_TRANSPORT_ADMISSION_HPP_INCLUDED
#define ERASED_TRANSPORT_ADMISSION_HPP_INCLUDED

#include "system/local_type_registry.hpp"
#include "system/system_context.hpp"
#include "system/system_id_registry.hpp"

namespace erased_transport_admission
{

[[nodiscard]] inline bool is_admissible(
    const type_id identity,
    const module_ids::id_type destination_module_id) noexcept
{
    const module_ids::id_type source_module_id =
        system_context::get_ambient_module_id();
    if (!module_ids::is_valid_id(source_module_id) ||
        !module_ids::is_valid_id(destination_module_id) ||
        !identity.is_valid())
    {
        return false;
    }

    system_type_id system_identity;
    if (identity.try_system_type_id(system_identity))
    {
        return system_id_registry::find_type(system_identity) != nullptr;
    }

    local_type_id local_identity;
    return (source_module_id == destination_module_id) &&
        identity.try_local_type_id(local_identity) &&
        (local_type_registry::find_type(local_identity) != nullptr);
}

}   //  namespace erased_transport_admission

#endif  //  ERASED_TRANSPORT_ADMISSION_HPP_INCLUDED
