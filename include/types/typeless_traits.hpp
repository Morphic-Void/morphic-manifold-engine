
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   typeless_traits.hpp
//  Author: Ritchie Brannan
//  Date:   14 May 2026
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Type-to-id binding traits for typeless storage.
//
//  Defines the project-level type-id specialisation point and registration
//  macro shared by owning typeless storage and inline POD typeless storage.

#pragma once

#ifndef TYPELESS_TRAITS_HPP_INCLUDED
#define TYPELESS_TRAITS_HPP_INCLUDED

#include <cstddef>      //  std::size_t

#include "system/system_ids.hpp"

//==============================================================================
//  Type-to-id binding
//==============================================================================

template<typename T>
struct TTypeId;

template<typename T>
inline constexpr std::size_t k_type_id_v = TTypeId<T>::value;

//==============================================================================
//  Registration macro
//==============================================================================

#define MV_DECLARE_TYPELESS(T, type_id)           \
template<>                                        \
struct TTypeId<T>                                 \
{                                                 \
    static constexpr std::size_t value = type_id; \
}

#endif  //  #ifndef TYPELESS_TRAITS_HPP_INCLUDED
