
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   typeless_pod.hpp
//  Author: Ritchie Brannan
//  Date:   14 May 2026
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Inline POD typeless storage.
//
//  Provides fixed-capacity erased storage for trivially copyable payloads
//  identified by project type ids.
//
//  This is a value mechanism only. It does not allocate, construct,
//  destroy, or transfer ownership.
//
//  The shared type-to-id binding traits are provided by
//  types/typeless_traits.hpp.

#pragma once

#ifndef TYPELESS_POD_HPP_INCLUDED
#define TYPELESS_POD_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstring>      //  std::memcpy, std::memset
#include <type_traits>  //  std::is_trivially_copyable_v

#include "types/typeless_traits.hpp"

//==============================================================================
//  TTypelessPod
//  Inline fixed-capacity POD typeless storage
//==============================================================================

template<std::size_t PayloadSize, std::size_t PayloadAlign>
class TTypelessPod
{
private:
    static_assert((PayloadSize > 0u), "TTypelessPod payload size must be non-zero.");
    static_assert((PayloadAlign > 0u), "TTypelessPod payload alignment must be non-zero.");
    static_assert(((PayloadAlign & (PayloadAlign - 1u)) == 0u), "TTypelessPod payload alignment must be a power of two.");

public:

    //  Default lifetime
    TTypelessPod() noexcept = default;
    TTypelessPod(const TTypelessPod&) noexcept = default;
    TTypelessPod& operator=(const TTypelessPod&) noexcept = default;
    ~TTypelessPod() noexcept = default;

    void clear() noexcept;
    bool is_empty() const noexcept;
    std::size_t query_type_id() const noexcept;

    template<typename T> static constexpr bool is_compatible_with() noexcept;

    template<typename T> bool is_a() const noexcept;
    template<typename T> bool assign(const T& value) noexcept;
    template<typename T> bool copy_to(T& out) const noexcept;

private:

    template<typename T> static constexpr void validate_payload_type() noexcept;

    void clear_payload() noexcept;

    std::size_t m_type_id = 0u;
    alignas(PayloadAlign) unsigned char m_payload[PayloadSize]{};
};

//==============================================================================
//  TTypelessPodFor
//  Convenience alias for storage shaped by a payload-layout type
//==============================================================================

template<typename TPayloadShape>
using TTypelessPodFor = TTypelessPod<sizeof(TPayloadShape), alignof(TPayloadShape)>;

//==============================================================================
//  TTypelessPod out of class function bodies
//==============================================================================

template<std::size_t PayloadSize, std::size_t PayloadAlign>
void TTypelessPod<PayloadSize, PayloadAlign>::clear() noexcept
{
    m_type_id = 0u;
    clear_payload();
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
bool TTypelessPod<PayloadSize, PayloadAlign>::is_empty() const noexcept
{
    return m_type_id == 0u;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
std::size_t TTypelessPod<PayloadSize, PayloadAlign>::query_type_id() const noexcept
{
    return m_type_id;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
constexpr bool TTypelessPod<PayloadSize, PayloadAlign>::is_compatible_with() noexcept
{
    return std::is_trivially_copyable_v<T>
        && (sizeof(T) <= PayloadSize)
        && (alignof(T) <= PayloadAlign);
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
bool TTypelessPod<PayloadSize, PayloadAlign>::is_a() const noexcept
{
    return m_type_id == k_type_id_v<T>;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
bool TTypelessPod<PayloadSize, PayloadAlign>::assign(const T& value) noexcept
{
    validate_payload_type<T>();

    clear_payload();
    std::memcpy(m_payload, &value, sizeof(T));
    m_type_id = k_type_id_v<T>;
    return true;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
bool TTypelessPod<PayloadSize, PayloadAlign>::copy_to(T& out) const noexcept
{
    validate_payload_type<T>();

    if (m_type_id != k_type_id_v<T>)
    {
        return false;
    }

    std::memcpy(&out, m_payload, sizeof(T));
    return true;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
constexpr void TTypelessPod<PayloadSize, PayloadAlign>::validate_payload_type() noexcept
{
    static_assert((k_type_id_v<T> != 0u), "TTypelessPod reserves type id zero for empty storage.");
    static_assert(std::is_trivially_copyable_v<T>, "TTypelessPod requires trivially copyable payloads.");
    static_assert((sizeof(T) <= PayloadSize), "TTypelessPod payload storage is too small.");
    static_assert((alignof(T) <= PayloadAlign), "TTypelessPod payload storage is under-aligned.");
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
void TTypelessPod<PayloadSize, PayloadAlign>::clear_payload() noexcept
{
    std::memset(m_payload, 0, PayloadSize);
}

#endif  //  #ifndef TYPELESS_POD_HPP_INCLUDED
