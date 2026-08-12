
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  Compile-time category ownership for system and local type-ID bindings.

#pragma once

#ifndef TYPE_ID_BINDING_CATEGORY_HPP_INCLUDED
#define TYPE_ID_BINDING_CATEGORY_HPP_INCLUDED

#include <cstdint>

enum class ETypeIdBindingCategory : std::uint8_t
{
    none = 0u,
    system,
    local
};

template<typename T>
struct TTypeIdBindingCategory
{
    static constexpr ETypeIdBindingCategory value = ETypeIdBindingCategory::none;
};

template<typename T>
inline constexpr ETypeIdBindingCategory k_type_id_binding_category_v = TTypeIdBindingCategory<T>::value;

template<typename T>
inline constexpr bool k_can_register_type_id_v = k_type_id_binding_category_v<T> == ETypeIdBindingCategory::none;

#endif  //  TYPE_ID_BINDING_CATEGORY_HPP_INCLUDED
