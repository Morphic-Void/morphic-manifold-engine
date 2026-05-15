
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TOwningEndpoints.hpp
//  Author: Ritchie Brannan
//  Date:   18 Apr 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Fixed-capacity SPSC ring transport endpoints for owned T.

#pragma once

#ifndef TOWNING_ENDPOINTS_HPP_INCLUDED
#define TOWNING_ENDPOINTS_HPP_INCLUDED

#include <utility>      //  std::move

#include "containers/TPodVector.hpp"
#include "threading/transports/core/TOwningTransport.hpp"
#include "threading/transports/interfaces/TOwningInterfaces.hpp"
#include "debug/debug.hpp"

namespace threading::transports
{

//==============================================================================
//  TOwningProducerEndpoint<T>
//  Single Producer, Single Consumer (SPSC) producer endpoint
//==============================================================================

template<typename T>
class TOwningProducerEndpoint : public TOwningProducerInterface<T>
{
public:
    explicit TOwningProducerEndpoint(TOwning<T>& ring) noexcept : m_ring{ &ring } {}
    ~TOwningProducerEndpoint() noexcept = default;

    //  Virtual overrides
    [[nodiscard]] virtual bool is_valid() const noexcept override final;
    [[nodiscard]] virtual bool is_ready() const noexcept override final;
    [[nodiscard]] virtual bool post(T&& src) noexcept override final;
    [[nodiscard]] virtual std::uint32_t writable_count() const noexcept override final;

private:
    TOwning<T>* m_ring = nullptr;
};

//==============================================================================
//  TOwningConsumerEndpoint<T>
//  Single Producer, Single Consumer (SPSC) consumer endpoint
//==============================================================================

template<typename T>
class TOwningConsumerEndpoint : public TOwningConsumerInterface<T>
{
public:
    explicit TOwningConsumerEndpoint(TOwning<T>& ring) noexcept : m_ring{ &ring } {}
    ~TOwningConsumerEndpoint() noexcept = default;

    //  Virtual overrides
    [[nodiscard]] virtual bool is_valid() const noexcept override final;
    [[nodiscard]] virtual bool is_ready() const noexcept override final;
    [[nodiscard]] virtual bool read(T& dst) noexcept override final;
    [[nodiscard]] virtual std::uint32_t readable_count() const noexcept override final;

private:
    TOwning<T>* m_ring = nullptr;
};

//==============================================================================
//  TOwningProducerEndpoint<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TOwningProducerEndpoint<T>::is_valid() const noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->posting_is_valid() : false;
}

template<typename T>
inline bool TOwningProducerEndpoint<T>::is_ready() const noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->is_ready() : false;
}

template<typename T>
inline bool TOwningProducerEndpoint<T>::post(T&& src) noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->post(std::move(src)) : false;
}

template<typename T>
inline std::uint32_t TOwningProducerEndpoint<T>::writable_count() const noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->writable_count() : 0u;
}

//==============================================================================
//  TOwningConsumerEndpoint<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TOwningConsumerEndpoint<T>::is_valid() const noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->reading_is_valid() : false;
}

template<typename T>
inline bool TOwningConsumerEndpoint<T>::is_ready() const noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->is_ready() : false;
}

template<typename T>
inline bool TOwningConsumerEndpoint<T>::read(T& dst) noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->read(dst) : false;
}

template<typename T>
inline std::uint32_t TOwningConsumerEndpoint<T>::readable_count() const noexcept
{
    MV_HARD_ASSERT(m_ring != nullptr);
    return (m_ring != nullptr) ? m_ring->readable_count() : 0u;
}

}   //  namespace threading::transports

#endif  //  #ifndef TOWNING_ENDPOINTS_HPP_INCLUDED

