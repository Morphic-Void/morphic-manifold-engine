
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   memory_typeless.hpp
//  Author: Ritchie Brannan
//  Date:   22 Feb 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Move-only erased ownership for one typed payload-family node.
//
//  Not a container and not a multi-object ownership mechanism.
//
//  IMPORTANT SEMANTIC NOTE
//  -----------------------
//  CTypeless emptiness is carrier-level emptiness only.
//
//  Payload semantic emptiness, if any, is defined by the recovered
//  payload type, not by CTypeless.
//
//  type_id() reports payload-family identity. Empty ownership reports 0.
//
//  Typed recovery is explicit and checked via typeless_cast<T, type_id>().
//
//  See docs/memory/memory_typeless.md for the full documentation.

#pragma once

#ifndef MEMORY_TYPELESS_HPP_INCLUDED
#define MEMORY_TYPELESS_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <type_traits>  //  std::is_nothrow_default_constructible_v et al.
#include <utility>      //  std::move
#include <new>          //  ::new

#include "memory_allocation.hpp"
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
    bool is_empty() const noexcept { return m_typeless == nullptr; }
    bool is_ready() const noexcept { return m_typeless != nullptr; }
    explicit operator bool() const noexcept { return m_typeless != nullptr; }

    //  Type identity
    std::size_t type_id() const noexcept { return (m_typeless != nullptr) ? m_typeless->type_id() : std::size_t{ 0u }; }

    //  Creation
    template<typename T, std::size_t type_id>
    static CTypeless create() noexcept;

    //  Teardown
    void destroy_and_deallocate() noexcept;

public:
    class ITypeless
    {
    public:
        virtual void destroy_and_deallocate() noexcept = 0;
        virtual std::size_t type_id() const noexcept = 0;

    protected:
        ~ITypeless() noexcept = default;
    };

private:
    ITypeless* m_typeless = nullptr;

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
    static constexpr std::size_t k_size = sizeof(TTypeless);
    static constexpr std::size_t k_align = alignof(TTypeless);

public:

    //  Default lifetime
    TTypeless() noexcept = default;

    //  Payload access
    T* payload_ptr() noexcept { return &m_payload; }
    const T* payload_ptr() const noexcept { return &m_payload; }

private:
    ~TTypeless() noexcept = default;

    void destroy_and_deallocate() noexcept override final
    {
        this->~TTypeless();
        byte_deallocate(this, k_align);
    }

    std::size_t type_id() const noexcept override final
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
    if ((typeless.m_typeless == nullptr) || (typeless.m_typeless->type_id() != type_id))
    {
        return nullptr;
    }
    return static_cast<TTypeless<T, type_id>*>(typeless.m_typeless)->payload_ptr();
}

template<typename T, std::size_t type_id>
const T* typeless_cast(const CTypeless& typeless) noexcept
{
    if ((typeless.m_typeless == nullptr) || (typeless.m_typeless->type_id() != type_id))
    {
        return nullptr;
    }
    return static_cast<TTypeless<T, type_id>*>(typeless.m_typeless)->payload_ptr();
}

//==============================================================================
//  CTypeless out of class function bodies
//==============================================================================

inline CTypeless::CTypeless(CTypeless&& typeless) noexcept
{
    m_typeless = typeless.m_typeless;
    typeless.m_typeless = nullptr;
}

inline CTypeless& CTypeless::operator=(CTypeless&& typeless) noexcept
{
    if (this != &typeless)
    {
        destroy_and_deallocate();
        m_typeless = typeless.m_typeless;
        typeless.m_typeless = nullptr;
    }
    return *this;
}

template<typename T, std::size_t type_id>
inline CTypeless CTypeless::create() noexcept
{
    using node_type = TTypeless<T, type_id>;
    CTypeless typeless;
    void* const memory = byte_allocate(node_type::k_size, node_type::k_align);
    MV_HARD_ASSERT(memory != nullptr);
    if (memory != nullptr)
    {
        typeless.m_typeless = ::new (memory) node_type();
    }
    return typeless;
}

inline void CTypeless::destroy_and_deallocate() noexcept
{
    if (m_typeless != nullptr)
    {
        m_typeless->destroy_and_deallocate();
        m_typeless = nullptr;
    }
}

}   //  namespace memory

#endif  //  #ifndef MEMORY_TYPELESS_HPP_INCLUDED