
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TObjectOwner.hpp
//  Author: Ritchie Brannan
//  Date:   01 Apr 26
//
//  Move-only owning single-object wrapper over TMemoryToken<T>.
//
//  Overview:
//  - TObjectOwner<T> owns storage for exactly one T.
//  - Non-empty state always implies that exactly one live T object is present.
//  - Construction is fused with acquisition.
//  - Destruction destroys the object and then releases the storage.
//  - Owner identity may remain stable across emplace(...).
//  - Object identity is not stable across emplace(...).
//
//  Scope:
//  - Single-object ownership only.
//  - No arrays.
//  - No custom deleters.
//  - No raw-pointer ownership release.
//  - No exposed allocated-but-unconstructed state.
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//  - T must be nothrow destructible.
//  - create(...) and emplace(...) require T to be nothrow constructible
//    from the supplied arguments.

#pragma once

#ifndef TOBJECT_OWNER_HPP_INCLUDED
#define TOBJECT_OWNER_HPP_INCLUDED

#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "memory/memory_primitives.hpp"

#include "debug/debug.hpp"

template<typename T>
class TObjectOwner final
{
    static_assert(!std::is_array_v<T>, "TObjectOwner<T> does not support array types.");
    static_assert(!std::is_reference_v<T>, "TObjectOwner<T> does not support reference types.");
    static_assert(!std::is_const_v<T>, "TObjectOwner<T> should not own const-qualified types.");
    static_assert(!std::is_volatile_v<T>, "TObjectOwner<T> should not own volatile-qualified types.");
    static_assert(std::is_nothrow_destructible_v<T>, "TObjectOwner<T> requires T to be nothrow destructible.");

public:

    //  Default and deleted lifetime
    TObjectOwner() noexcept = default;
    TObjectOwner(const TObjectOwner&) = delete;
    TObjectOwner& operator=(const TObjectOwner&) = delete;

    //  Move lifetime
    TObjectOwner(TObjectOwner&& other) noexcept : m_token(std::move(other.m_token)) {}
    TObjectOwner& operator=(TObjectOwner&& other) noexcept;

    //  Destructor
    ~TObjectOwner() noexcept { destroy_and_deallocate(); }

    //  Status
    [[nodiscard]] bool is_empty() const noexcept { return m_token.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_token.is_ready(); }
    [[nodiscard]] explicit operator bool() const noexcept { return m_token.is_ready(); }

    //  Accessors
    [[nodiscard]] T& operator*() noexcept;
    [[nodiscard]] const T& operator*() const noexcept;
    [[nodiscard]] T* operator->() noexcept;
    [[nodiscard]] const T* operator->() const noexcept;

    //  Content management
    template<typename... TArgs> [[nodiscard]] static TObjectOwner create(TArgs&&... args) noexcept;
    template<typename... TArgs> bool emplace(TArgs&&... args) noexcept;
    void reset() noexcept;
    void swap(TObjectOwner& other) noexcept;

private:
    void destroy_and_deallocate() noexcept;

    memory::TMemoryToken<T> m_token;
};

//==============================================================================
//  TObjectOwner<T> non-member helper functions
//==============================================================================

template<typename T>
inline void swap(TObjectOwner<T>& lhs, TObjectOwner<T>& rhs) noexcept
{   //  exchange payloads
    lhs.swap(rhs);
}

template<typename T, typename... TArgs>
inline TObjectOwner<T> make_object_owner(TArgs&&... args) noexcept
{   //  factory
    return TObjectOwner<T>::create(std::forward<TArgs>(args)...);
}

//==============================================================================
//  TObjectOwner<T> out of class function bodies
//==============================================================================

template<typename T>
inline TObjectOwner<T>& TObjectOwner<T>::operator=(TObjectOwner<T>&& other) noexcept
{
    if (this != &other)
    {
        destroy_and_deallocate();
        m_token = std::move(other.m_token);
    }
    return *this;
}

template<typename T>
inline T& TObjectOwner<T>::operator*() noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return *m_token.data();
}

template<typename T>
inline const T& TObjectOwner<T>::operator*() const noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return *m_token.data();
}

template<typename T>
inline T* TObjectOwner<T>::operator->() noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return m_token.data();
}

template<typename T>
inline const T* TObjectOwner<T>::operator->() const noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return m_token.data();
}

template<typename T>
template<typename... TArgs>
inline TObjectOwner<T> TObjectOwner<T>::create(TArgs&&... args) noexcept
{
    static_assert(std::is_nothrow_constructible_v<T, TArgs&&...>,
        "TObjectOwner<T>::create(...) requires T to be nothrow constructible.");

    TObjectOwner owner;
    (void)owner.emplace(std::forward<TArgs>(args)...);
    return owner;
}

template<typename T>
template<typename... TArgs>
inline bool TObjectOwner<T>::emplace(TArgs&&... args) noexcept
{
    static_assert(std::is_nothrow_constructible_v<T, TArgs&&...>,
        "TObjectOwner<T>::emplace(...) requires T to be nothrow constructible.");

    T* ptr = m_token.data();
    if (ptr != nullptr)
    {   //  existing storage - preserve owner identity, deconstruct and reconstruct in-place
        ptr->~T();
    }
    else if (m_token.allocate(1u))
    {   //  storage allocated
        ptr = m_token.data();
    }
    else
    {
        return false;
    }
    ::new (static_cast<void*>(ptr)) T(std::forward<TArgs>(args)...);
    return true;
}

template<typename T>
inline void TObjectOwner<T>::destroy_and_deallocate() noexcept
{
    T* const ptr = m_token.data();
    if (ptr != nullptr)
    {
        ptr->~T();
    }

    m_token.deallocate();
}

template<typename T>
inline void TObjectOwner<T>::reset() noexcept
{
    destroy_and_deallocate();
}

template<typename T>
inline void TObjectOwner<T>::swap(TObjectOwner& other) noexcept
{
    using std::swap;
    swap(m_token, other.m_token);
}

#endif  //  TOBJECT_OWNER_HPP_INCLUDED

