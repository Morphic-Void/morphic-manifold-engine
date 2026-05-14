
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   typeless.hpp
//  Author: Ritchie Brannan
//  Date:   14 May 2026
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Type-directed owning typeless helpers.
//
//  Bridges project-level fixed type ids onto the lower-level owning
//  typeless mechanism.
//
//  The shared type-to-id binding traits are provided by
//  types/typeless_traits.hpp.
//
//  Inline POD typeless storage is provided separately by
//  types/typeless_pod.hpp.

#pragma once

#ifndef TYPELESS_HPP_INCLUDED
#define TYPELESS_HPP_INCLUDED

#include "memory/memory_typeless.hpp"
#include "types/typeless_traits.hpp"

//==============================================================================
//  Type-directed owning typeless helpers
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

#endif  //  #ifndef TYPELESS_HPP_INCLUDED
