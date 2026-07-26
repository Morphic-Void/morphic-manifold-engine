
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   system_type_registration.hpp
//  Author: Ritchie Brannan
//  Date:   26 Jul 26
//
//  C++ type binding for generated system type ids and explicit erased-owner
//  payload eligibility.

#pragma once

#ifndef SYSTEM_TYPE_REGISTRATION_HPP_INCLUDED
#define SYSTEM_TYPE_REGISTRATION_HPP_INCLUDED

#include <type_traits>  //  std::false_type, std::true_type

#include "system/system_ids.hpp"

//==============================================================================
//  Type-to-id binding
//==============================================================================

template<typename T>
struct TTypeId;

template<typename T>
inline constexpr type_ids::id_type k_type_id_v = TTypeId<T>::value;

//==============================================================================
//  Erased-owner payload eligibility
//==============================================================================

template<typename T>
struct TIsErasedOwnerPayload : std::false_type {};

template<typename T>
inline constexpr bool k_is_erased_owner_payload_v = TIsErasedOwnerPayload<T>::value;

//==============================================================================
//  Registration macros
//==============================================================================

#define MV_REGISTER_SYSTEM_TYPE(T, type_id)       \
template<>                                        \
struct TTypeId<T>                                 \
{                                                 \
    static constexpr type_ids::id_type value = type_id; \
}

#define MV_REGISTER_ERASED_OWNER_PAYLOAD(T)       \
template<>                                        \
struct TIsErasedOwnerPayload<T> : std::true_type  \
{                                                 \
}

#endif  //  #ifndef SYSTEM_TYPE_REGISTRATION_HPP_INCLUDED
