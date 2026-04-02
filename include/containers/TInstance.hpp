
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TInstance.hpp
//  Author: Ritchie Brannan
//  Date:   01 Apr 26
//
//  Move-only owning single-object wrapper over TMemoryToken<T>.
//
//  TInstance<T> is the unique owning wrapper for a single constructed T.
//  Non-empty state implies exactly one live object.
//
//  Overview:
//  - TInstance<T> owns storage for exactly one T.
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

#ifndef TINSTANCE_HPP_INCLUDED
#define TINSTANCE_HPP_INCLUDED

#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "memory/memory_primitives.hpp"

#include "debug/debug.hpp"

template<typename T>
class TInstance final
{
    static_assert(!std::is_array_v<T>, "TInstance<T> does not support array types.");
    static_assert(!std::is_reference_v<T>, "TInstance<T> does not support reference types.");
    static_assert(!std::is_const_v<T>, "TInstance<T> should not own const-qualified types.");
    static_assert(!std::is_volatile_v<T>, "TInstance<T> should not own volatile-qualified types.");
    static_assert(std::is_nothrow_destructible_v<T>, "TInstance<T> requires T to be nothrow destructible.");

public:

    //  Default and deleted lifetime
    TInstance() noexcept = default;
    TInstance(const TInstance&) = delete;
    TInstance& operator=(const TInstance&) = delete;

    //  Move lifetime
    TInstance(TInstance&& other) noexcept : m_token(std::move(other.m_token)) {}
    TInstance& operator=(TInstance&& other) noexcept;

    //  Destructor
    ~TInstance() noexcept { destroy_and_deallocate(); }

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
    template<typename... TArgs> [[nodiscard]] static TInstance create(TArgs&&... args) noexcept;
    template<typename... TArgs> bool emplace(TArgs&&... args) noexcept;
    void reset() noexcept;
    void swap(TInstance& other) noexcept;

private:
    void destroy_and_deallocate() noexcept;

    memory::TMemoryToken<T> m_token;
};

//==============================================================================
//  TInstance<T> non-member helper functions
//==============================================================================

template<typename T>
inline void swap(TInstance<T>& lhs, TInstance<T>& rhs) noexcept
{   //  exchange payloads
    lhs.swap(rhs);
}

template<typename T, typename... TArgs>
inline TInstance<T> make_object_owner(TArgs&&... args) noexcept
{   //  factory
    return TInstance<T>::create(std::forward<TArgs>(args)...);
}

//==============================================================================
//  TInstance<T> out of class function bodies
//==============================================================================

template<typename T>
inline TInstance<T>& TInstance<T>::operator=(TInstance<T>&& other) noexcept
{
    if (this != &other)
    {
        destroy_and_deallocate();
        m_token = std::move(other.m_token);
    }
    return *this;
}

template<typename T>
inline T& TInstance<T>::operator*() noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return *m_token.data();
}

template<typename T>
inline const T& TInstance<T>::operator*() const noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return *m_token.data();
}

template<typename T>
inline T* TInstance<T>::operator->() noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return m_token.data();
}

template<typename T>
inline const T* TInstance<T>::operator->() const noexcept
{
    MV_HARD_ASSERT(m_token.data() != nullptr);
    return m_token.data();
}

template<typename T>
template<typename... TArgs>
inline TInstance<T> TInstance<T>::create(TArgs&&... args) noexcept
{
    static_assert(std::is_nothrow_constructible_v<T, TArgs&&...>,
        "TInstance<T>::create(...) requires T to be nothrow constructible.");

    TInstance owner;
    (void)owner.emplace(std::forward<TArgs>(args)...);
    return owner;
}

template<typename T>
template<typename... TArgs>
inline bool TInstance<T>::emplace(TArgs&&... args) noexcept
{
    static_assert(std::is_nothrow_constructible_v<T, TArgs&&...>,
        "TInstance<T>::emplace(...) requires T to be nothrow constructible.");

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
inline void TInstance<T>::destroy_and_deallocate() noexcept
{
    T* const ptr = m_token.data();
    if (ptr != nullptr)
    {
        ptr->~T();
    }

    m_token.deallocate();
}

template<typename T>
inline void TInstance<T>::reset() noexcept
{
    destroy_and_deallocate();
}

template<typename T>
inline void TInstance<T>::swap(TInstance& other) noexcept
{
    using std::swap;
    swap(m_token, other.m_token);
}

#endif  //  TINSTANCE_HPP_INCLUDED

