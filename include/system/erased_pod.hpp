
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
//  Provides fixed-capacity erased storage for POD payloads identified by
//  project type ids. Payloads may be value-initialised in place and accessed
//  directly without copying.
//
//  This is a value mechanism only. Redefinition begins the lifetime of a
//  trivial payload in place; it does not allocate or transfer ownership.
//
//  The shared type-to-id binding is provided by
//  system/system_type_registration.hpp.

#pragma once

#ifndef ERASED_POD_HPP_INCLUDED
#define ERASED_POD_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstring>      //  std::memset
#include <new>          //  placement new, std::launder
#include <type_traits>  //  std::is_standard_layout_v, std::is_trivial_v

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

    [[nodiscard]] type_ids::id_type query_type_id() const noexcept;

    template<typename T> static constexpr bool is_compatible_with() noexcept;

    template<typename T> [[nodiscard]] bool is_a() const noexcept;

    //  Redefinition clears every byte after the type id through the end of
    //  payload storage, then begins and value-initialises the POD in place.
    template<typename T> [[nodiscard]] T& redefine() noexcept;

    //  Typed pointers remain valid until the storage is redefined.
    template<typename T> [[nodiscard]] T* payload() noexcept;
    template<typename T> [[nodiscard]] const T* payload() const noexcept;

private:

    template<typename T> static constexpr void validate_payload_type() noexcept;

    type_ids::id_type m_type_id{ type_ids::undefined };
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
type_ids::id_type TErasedPod<PayloadSize, PayloadAlign>::query_type_id() const noexcept
{
    return m_type_id;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
constexpr bool TErasedPod<PayloadSize, PayloadAlign>::is_compatible_with() noexcept
{
    return std::is_trivial_v<T>
        && std::is_standard_layout_v<T>
        && (sizeof(T) <= PayloadSize)
        && (alignof(T) <= PayloadAlign);
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
bool TErasedPod<PayloadSize, PayloadAlign>::is_a() const noexcept
{
    validate_payload_type<T>();
    return m_type_id == k_type_id_v<T>;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
T& TErasedPod<PayloadSize, PayloadAlign>::redefine() noexcept
{
    validate_payload_type<T>();
    std::memset(
        (reinterpret_cast<unsigned char*>(this) + sizeof(m_type_id)), 0,
        (sizeof(TErasedPod<PayloadSize, PayloadAlign>) - sizeof(m_type_id)));
    T* const value = ::new (static_cast<void*>(m_payload)) T{};
    m_type_id = k_type_id_v<T>;
    return *value;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
T* TErasedPod<PayloadSize, PayloadAlign>::payload() noexcept
{
    validate_payload_type<T>();
    return is_a<T>() ? std::launder(reinterpret_cast<T*>(m_payload)) : nullptr;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
const T* TErasedPod<PayloadSize, PayloadAlign>::payload() const noexcept
{
    validate_payload_type<T>();
    return is_a<T>() ? std::launder(reinterpret_cast<const T*>(m_payload)) : nullptr;
}

template<std::size_t PayloadSize, std::size_t PayloadAlign>
template<typename T>
constexpr void TErasedPod<PayloadSize, PayloadAlign>::validate_payload_type() noexcept
{
    static_assert(type_ids::is_valid_id(k_type_id_v<T>), "TErasedPod requires a valid, non-zero payload type id.");
    static_assert(std::is_trivial_v<T>, "TErasedPod requires trivial payloads.");
    static_assert(std::is_standard_layout_v<T>, "TErasedPod requires standard-layout payloads.");
    static_assert((sizeof(T) <= PayloadSize), "TErasedPod payload storage is too small.");
    static_assert((alignof(T) <= PayloadAlign), "TErasedPod payload storage is under-aligned.");
}

#endif  //  #ifndef ERASED_POD_HPP_INCLUDED
