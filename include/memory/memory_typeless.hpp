
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   memory_typeless.hpp
//  Author: Ritchie Brannan
//  Date:   22 Feb 26
//
//  Move-only erased ownership for one typed payload-family node.
//
//  Provides:
//  - CTypeless, the erased owning carrier;
//  - TTypeless<T, type_id>, the typed payload node wrapper;
//  - checked typed recovery through typeless_cast<T, type_id>().
//
//  Does not provide:
//  - general container semantics;
//  - multi-object ownership;
//  - payload semantic emptiness;
//  - deep allocation accounting;
//  - unchecked type recovery;
//  - a general runtime type system or plugin ABI.
//
//  Cross-header ownership, attribution, and accounting policy is documented
//  in docs/memory/memory_subsystem.md.
//
//  Typeless-specific payload-family, recovery, and usage guidance is documented
//  in docs/memory/memory_typeless.md.

#pragma once

#ifndef MEMORY_TYPELESS_HPP_INCLUDED
#define MEMORY_TYPELESS_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <type_traits>  //  std::is_nothrow_default_constructible_v et al.
#include <utility>      //  std::move
#include <new>          //  ::new

#include "memory_token.hpp"
#include "debug/debug.hpp"

namespace memory
{

//==============================================================================
//  CTypeless
//==============================================================================

class CTypeless
{
public:

    //  Default, move-only lifetime
    CTypeless() noexcept = default;
    CTypeless(const CTypeless&) = delete;
    CTypeless& operator=(const CTypeless&) = delete;
    CTypeless(CTypeless&&) noexcept;
    CTypeless& operator=(CTypeless&&) noexcept;
    ~CTypeless() noexcept { destroy_and_deallocate(); }

    //  Ownership state
    bool is_empty() const noexcept { return node() == nullptr; }
    bool is_ready() const noexcept { return node() != nullptr; }
    explicit operator bool() const noexcept { return node() != nullptr; }

    //  Type identity
    std::size_t query_type_id() const noexcept { return is_ready() ? node()->query_type_id() : std::size_t{ 0u }; }

    //  Creation
    template<typename T, std::size_t type_id>
    static CTypeless create() noexcept;

    //  Teardown
    void destroy_and_deallocate() noexcept;

public:
    class ITypeless
    {
    public:
        virtual void destroy() noexcept = 0;
        virtual std::size_t query_type_id() const noexcept = 0;

    protected:
        ~ITypeless() noexcept = default;
    };

private:
    ITypeless* node() noexcept { return static_cast<ITypeless*>(m_storage.data()); }
    const ITypeless* node() const noexcept { return static_cast<const ITypeless*>(m_storage.data()); }

    CMemoryToken m_storage;

private:
    template<typename T, std::size_t type_id>
    friend T* typeless_cast(CTypeless&) noexcept;

    template<typename T, std::size_t type_id>
    friend const T* typeless_cast(const CTypeless&) noexcept;
};

//==============================================================================
//  TTypeless
//==============================================================================

template<typename T, std::size_t type_id>
class TTypeless final : public CTypeless::ITypeless
{
    static_assert(std::is_nothrow_default_constructible_v<T>, "TTypeless<T> requires T to be nothrow default constructable.");
    static_assert(std::is_nothrow_move_constructible_v<T>, "TTypeless<T> requires T to be nothrow move constructable.");
    static_assert(std::is_nothrow_move_assignable_v<T>, "TTypeless<T> requires T to be nothrow assignable.");
    static_assert(std::is_nothrow_destructible_v<T>, "TTypeless<T> requires T to be nothrow destructible.");

public:
    static constexpr std::size_t k_type_id = type_id;

public:

    //  Default lifetime
    TTypeless() noexcept = default;

    //  Payload access
    T* payload_ptr() noexcept { return &m_payload; }
    const T* payload_ptr() const noexcept { return &m_payload; }

private:
    ~TTypeless() noexcept = default;

    void destroy() noexcept override final
    {
        this->~TTypeless();
    }

    std::size_t query_type_id() const noexcept override final
    {
        return k_type_id;
    }

    T m_payload = {};
};

//==============================================================================
//  Typed recovery helpers
//==============================================================================

template<typename T, std::size_t type_id>
T* typeless_cast(CTypeless& typeless) noexcept
{
    if (!typeless.is_ready() || (typeless.node()->query_type_id() != type_id))
    {
        return nullptr;
    }
    return static_cast<TTypeless<T, type_id>*>(typeless.node())->payload_ptr();
}

template<typename T, std::size_t type_id>
const T* typeless_cast(const CTypeless& typeless) noexcept
{
    if (!typeless.is_ready() || (typeless.node()->query_type_id() != type_id))
    {
        return nullptr;
    }
    return static_cast<const TTypeless<T, type_id>*>(typeless.node())->payload_ptr();
}

//==============================================================================
//  CTypeless out of class function bodies
//==============================================================================

inline CTypeless::CTypeless(CTypeless&& typeless) noexcept
    : m_storage(std::move(typeless.m_storage))
{
}

inline CTypeless& CTypeless::operator=(CTypeless&& typeless) noexcept
{
    if (this != &typeless)
    {
        destroy_and_deallocate();
        m_storage = std::move(typeless.m_storage);
    }
    return *this;
}

template<typename T, std::size_t type_id>
inline CTypeless CTypeless::create() noexcept
{
    using node_type = TTypeless<T, type_id>;
    CTypeless typeless;
    const bool configured = typeless.m_storage.configure_relocatable(1u, alignof(node_type));
    const bool allocated = configured && typeless.m_storage.allocate(sizeof(node_type), false);
    MV_HARD_ASSERT(allocated);
    if (allocated)
    {
        ::new (typeless.m_storage.data()) node_type();
    }
    return typeless;
}

inline void CTypeless::destroy_and_deallocate() noexcept
{
    if (ITypeless* const typeless = node())
    {
        typeless->destroy();
    }
    m_storage.deallocate();
}

}   //  namespace memory

#endif  //  #ifndef MEMORY_TYPELESS_HPP_INCLUDED
