
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  Host-owned canonical system identity name tables.

#pragma once

#ifndef HOST_SYSTEM_ID_DEFINITIONS_HPP_INCLUDED
#define HOST_SYSTEM_ID_DEFINITIONS_HPP_INCLUDED

#include "system/system_id_registry.hpp"

namespace host
{

[[nodiscard]] const system_id_registry::SSystemRegistryView& system_registry_view() noexcept;

}   //  namespace host

#endif  //  HOST_SYSTEM_ID_DEFINITIONS_HPP_INCLUDED
