
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   typeless.hpp
//  Author: Ritchie Brannan
//  Date:   [date]
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Type-to-id binding and convenience helpers for CTypeless.
//
//  Bridges project-level fixed type ids onto the lower-level
//  typeless ownership mechanism.
//
//  Defines TTypeId<T> specialisation points, registration helpers,
//  and type-directed create/cast convenience functions.
//
//  This is a binding layer, not the typeless mechanism itself.

#pragma once

#ifndef TYPELESS_HPP_INCLUDED
#define TYPELESS_HPP_INCLUDED

#include <cstddef>      //  std::size_t

#include "memory/memory_typeless.hpp"
#include "system/system_ids.hpp"

//==============================================================================
//  Type-to-id binding
//==============================================================================

template<typename T>
struct TTypeId;

template<typename T>
inline constexpr std::size_t k_type_id_v = TTypeId<T>::value;

//==============================================================================
//  Type-directed typeless helpers
//==============================================================================

template<typename T>
inline memory::CTypeless create_typeless() noexcept
{
    return memory::CTypeless::create<T, k_type_id_v<T>>();
}

template<typename T>
inline T* typeless_cast(memory::CTypeless& typeless) noexcept
{
    return memory::typeless_cast<T, k_type_id_v<T>>(typeless);
}

template<typename T>
inline const T* typeless_cast(const memory::CTypeless& typeless) noexcept
{
    return memory::typeless_cast<T, k_type_id_v<T>>(typeless);
}

//==============================================================================
//  Registration macro
//==============================================================================

#define MV_DECLARE_TYPELESS(T, type_id)           \
template<>                                        \
struct TTypeId<T>                                 \
{                                                 \
    static constexpr std::size_t value = type_id; \
}

#endif
