
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   erased_pod.hpp
//  Author: Ritchie Brannan
//  Date:   14 May 2026
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Inline POD erased storage.
//
//  Provides fixed-capacity erased storage for trivially copyable payloads
//  identified by project type ids.
//
//  This is a value mechanism only. It does not allocate, construct,
//  destroy, or transfer ownership.
//
//  The shared type-to-id binding is provided by
//  system/system_type_registration.hpp.

#pragma once

#ifndef ERASED_POD_HPP_INCLUDED
#define ERASED_POD_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstring>      //  std::memcpy, std::memset
#include <type_traits>  //  std::is_trivially_copyable_v

#include "system/system_type_registration.hpp"

//==============================================================================
//  TErasedPod
//  Inline fixed-capacity POD erased storage
//==============================================================================

template<std::size_t PayloadSize, std::size_t PayloadAlign>
class TErasedPod
{
private:
    static_assert((PayloadSize > 0u), "TErasedPod payload size must be non-zero.");
    static_assert((PayloadAlign > 0u), "TErasedPod payload alignment must be non-zero.");
    static_assert(((PayloadAlign & (PayloadAlign - 1u)) == 0u), "TErasedPod payload alignment must be a power of two.");

public:

    //  Default lifetime
    TErasedPod() noexcept = default;
    TErasedPod(const TErasedPod&) noexcept = default;
    TErasedPod& operator=(const TErasedPod&) noexcept = default;
    ~TErasedPod() noexcept = default;

    void clear() noexcept;
    bool is_empty() const noexcept;
    type_ids::id_type query_type_id() const noexcept;

    template<typename T> static constexpr bool is_compatible_with() noexcept;

    template<typename T> bool is_a() const noexcept;
    template<typename T> bool assign(const T& value) noexcept;
    template<typename T> bool copy_to(T& out) const noexcept;

private:

    template<typename T> static constexpr void validate_payload_type() noexcept;

    void clear_payload() noexcept;

    type_ids::id_type m_type_id{};
    alignas(PayloadAlign) unsigned char m_payload[PayloadSize]{};
};

//==============================================================================
//  TErasedPodFor
//  Convenience alias for storage shaped by a payload-layout type
//==============================================================================

template<typename TPayloadShape>
using TErasedPodFor = TErasedPod<sizeof(TPayloadShape), alignof(TPayloadShape)>;

//==============================================================================
//  TErasedPod out of class function bodies
//==============================================================================

template<std::size_t PayloadSize, std::size_t PayloadAlign>
void TErasedPod<PayloadSize, PayloadAlign>::clear() noexcept
{
    m_type_id = type_ids::undefined;
    clear_payload();
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
bool TErasedPod<PayloadSize, PayloadAlign>::is_empty() const noexcept
{
    return m_type_id == type_ids::undefined;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
type_ids::id_type TErasedPod<PayloadSize, PayloadAlign>::query_type_id() const noexcept
{
    return m_type_id;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
constexpr bool TErasedPod<PayloadSize, PayloadAlign>::is_compatible_with() noexcept
{
    return std::is_trivially_copyable_v<T>
        && (sizeof(T) <= PayloadSize)
        && (alignof(T) <= PayloadAlign);
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
bool TErasedPod<PayloadSize, PayloadAlign>::is_a() const noexcept
{
    return m_type_id == k_type_id_v<T>;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
bool TErasedPod<PayloadSize, PayloadAlign>::assign(const T& value) noexcept
{
    validate_payload_type<T>();

    clear_payload();
    std::memcpy(m_payload, &value, sizeof(T));
    m_type_id = k_type_id_v<T>;
    return true;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
bool TErasedPod<PayloadSize, PayloadAlign>::copy_to(T& out) const noexcept
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
constexpr void TErasedPod<PayloadSize, PayloadAlign>::validate_payload_type() noexcept
{
    static_assert(type_ids::is_valid_id(k_type_id_v<T>), "TErasedPod requires a valid, non-zero payload type id.");
    static_assert(std::is_trivially_copyable_v<T>, "TErasedPod requires trivially copyable payloads.");
    static_assert((sizeof(T) <= PayloadSize), "TErasedPod payload storage is too small.");
    static_assert((alignof(T) <= PayloadAlign), "TErasedPod payload storage is under-aligned.");
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
void TErasedPod<PayloadSize, PayloadAlign>::clear_payload() noexcept
{
    std::memset(m_payload, 0, PayloadSize);
}

#endif  //  #ifndef ERASED_POD_HPP_INCLUDED
