
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TRingTransport.hpp
//  Author: Ritchie Brannan
//  Date:   01 Apr 26

#pragma once

#ifndef TRING_TRANSPORT_HPP_INCLUDED
#define TRING_TRANSPORT_HPP_INCLUDED

#include <algorithm>    //  std::max
#include <atomic>       //  std::atomic
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint32_t
#include <cstring>      //  std::memcpy
#include <type_traits>  //  std::is_const_v, std::is_trivially_copyable_v

#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "bit_utils/bit_ops.hpp"

namespace threading::transports
{

//==============================================================================
//  TRing<T>
//  Single Producer, Single Consumer (SPSC) transport
//==============================================================================

template<typename T>
class TRing
{
private:
    static_assert(!std::is_const_v<T>, "TRing<T> requires non-const T.");
    static_assert(std::is_trivially_copyable_v<T>, "TRing<T> requires trivially copyable T.");

public:
    static constexpr std::uint32_t k_max_capacity = 0x00100000u;    //  approximately 1 million elements
    static constexpr std::uint32_t k_min_capacity = 32u;

public:
    TRing() noexcept = default;
    TRing(const TRing&) = delete;
    TRing& operator=(const TRing&) = delete;
    TRing(TRing&&) noexcept = delete;
    TRing& operator=(TRing&&) noexcept = delete;
    ~TRing() noexcept { (void)deallocate(); }

    //  Status
    [[nodiscard]] bool producer_is_valid() const noexcept;
    [[nodiscard]] bool consumer_is_valid() const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;

    //  Producer operations
    bool post(const T& src) noexcept { return post(&src, 1u); }
    bool post(const T* const src, const std::uint32_t count = 1u) noexcept;
    bool post(const TPodConstView<T>& src) noexcept { return post(src.data(), src.size()); }
    [[nodiscard]] std::uint32_t writable_count() const noexcept;

    //  Consumer operations
    bool read(T& dst) noexcept { return read(&dst, 1u); }
    bool read(T* const dst, const std::uint32_t count = 1u) noexcept;
    bool read(const TPodView<T>& dst) noexcept { return read(dst.data(), dst.size()); }
    [[nodiscard]] std::uint32_t readable_count() const noexcept;

    //  Setup and teardown
    [[nodiscard]] bool initialise(const std::uint32_t capacity) noexcept;
    void deallocate() noexcept;

private:
    memory::TMemoryToken<T> m_ring;
    std::uint32_t m_capacity = 0u;
    std::uint32_t m_read_index = 0u;
    std::uint32_t m_write_index = 0u;
    std::atomic<std::int32_t> m_occupied_count{ 0 };
};

//==============================================================================
//  TRing<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TRing<T>::producer_is_valid() const noexcept
{
    return is_valid() ? ((m_capacity != 0u) ? (m_write_index < m_capacity) : (m_write_index == 0u)) : false;
}

template<typename T>
inline bool TRing<T>::consumer_is_valid() const noexcept
{
    return is_valid() ? ((m_capacity != 0u) ? (m_read_index < m_capacity) : (m_read_index == 0u)) : false;
}

template<typename T>
inline bool TRing<T>::is_valid() const noexcept
{
    if (m_capacity == 0u)
    {   //  uninitialised, safe to check both the read and the write indices
        if ((m_ring.data() != nullptr) || ((m_read_index | m_write_index) != 0u))
        {
            return false;
        }
    }
    else if (m_ring.data() == nullptr)
    {
        return false;
    }
    const std::uint32_t count = static_cast<std::uint32_t>(m_occupied_count.load(std::memory_order_acquire));
    return count <= m_capacity;
}

template<typename T>
inline bool TRing<T>::is_ready() const noexcept
{
    return (m_capacity != 0u) && (m_ring.data() != nullptr);
}

template<typename T>
inline bool TRing<T>::post(const T* const src, const std::uint32_t count) noexcept
{
    if (!is_ready() || (count > writable_count()) || ((src == nullptr) && (count != 0u)))
    {
        return false;
    }
    if (count != 0u)
    {
        const std::uint32_t tail_size = m_capacity - m_write_index;
        if (count <= tail_size)
        {
            std::memcpy((m_ring.data() + m_write_index), src, (static_cast<std::size_t>(count) * sizeof(T)));
            m_write_index = (m_write_index + count) & (m_capacity - 1u);
        }
        else
        {
            std::memcpy((m_ring.data() + m_write_index), src, (static_cast<std::size_t>(tail_size) * sizeof(T)));
            m_write_index = count - tail_size;
            std::memcpy(m_ring.data(), (src + tail_size), (static_cast<std::size_t>(m_write_index) * sizeof(T)));
        }
        m_occupied_count.fetch_add(static_cast<std::int32_t>(count), std::memory_order_release);
    }
    return true;
}

template<typename T>
inline std::uint32_t TRing<T>::writable_count() const noexcept
{
    const std::uint32_t count = static_cast<std::uint32_t>(m_occupied_count.load(std::memory_order_acquire));
    return (count <= m_capacity ) ? (m_capacity - count) : 0u;
}

template<typename T>
inline bool TRing<T>::read(T* const dst, const std::uint32_t count) noexcept
{
    if (!is_ready() || (count > readable_count()) || ((dst == nullptr) && (count != 0u)))
    {
        return false;
    }
    if (count != 0u)
    {
        const std::uint32_t tail_size = m_capacity - m_read_index;
        if (count <= tail_size)
        {
            std::memcpy(dst, (m_ring.data() + m_read_index), (static_cast<std::size_t>(count) * sizeof(T)));
            m_read_index = (m_read_index + count) & (m_capacity - 1u);
        }
        else
        {
            std::memcpy(dst, (m_ring.data() + m_read_index), (static_cast<std::size_t>(tail_size) * sizeof(T)));
            m_read_index = count - tail_size;
            std::memcpy((dst + tail_size), m_ring.data(), (static_cast<std::size_t>(m_read_index) * sizeof(T)));
        }
        m_occupied_count.fetch_add(-static_cast<std::int32_t>(count), std::memory_order_release);
    }
    return true;
}

template<typename T>
inline std::uint32_t TRing<T>::readable_count() const noexcept
{
    const std::uint32_t count = static_cast<std::uint32_t>(m_occupied_count.load(std::memory_order_acquire));
    return (count <= m_capacity) ? count : 0u;
}

template<typename T>
inline bool TRing<T>::initialise(const std::uint32_t capacity) noexcept
{
    if (capacity > k_max_capacity)
    {   //  requested capacity not supported
        return false;
    }
    if (m_capacity != 0u)
    {   //  re-initialisation is not allowed without deallocation
        return false;
    }
    const std::uint32_t conditioned_capacity = std::max(std::min(bit_ops::round_up_to_pow2(capacity), k_max_capacity), k_min_capacity);
    if (!m_ring.allocate(conditioned_capacity))
    {   //  allocation failed
        return false;
    }
    m_capacity = conditioned_capacity;
    m_read_index = 0u;
    m_write_index = 0u;
    m_occupied_count.store(0, std::memory_order_release);
    return true;
}

template<typename T>
inline void TRing<T>::deallocate() noexcept
{
    m_ring.deallocate();
    m_capacity = 0u;
    m_read_index = 0u;
    m_write_index = 0u;
    m_occupied_count.store(0, std::memory_order_release);
}

}   //  namespace threading::transports

#endif  //  #ifndef TRING_TRANSPORT_HPP_INCLUDED

