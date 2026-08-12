//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  C++ type binding for component-local type identities.

#pragma once

#ifndef LOCAL_TYPE_REGISTRATION_HPP_INCLUDED
#define LOCAL_TYPE_REGISTRATION_HPP_INCLUDED

#include "system/system_ids.hpp"
#include "system/type_id_binding_category.hpp"

template<typename T>
struct TLocalTypeId;

template<typename T>
inline constexpr local_type_ids::id_type k_local_type_id_v = TLocalTypeId<T>::value;

#define MV_REGISTER_LOCAL_TYPE(T, type_id_value) \
template<> \
struct TTypeIdBindingCategory<T> \
{ \
    static constexpr ETypeIdBindingCategory value = ETypeIdBindingCategory::local; \
}; \
template<> \
struct TLocalTypeId<T> \
{ \
    static_assert(local_type_ids::is_valid_id(type_id_value), \
        "MV_REGISTER_LOCAL_TYPE requires a valid local type id."); \
    static constexpr local_type_ids::id_type value = type_id_value; \
}

#endif  //  LOCAL_TYPE_REGISTRATION_HPP_INCLUDED
