
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TQueueTransport.hpp
//  Author: Ritchie Brannan
//  Date:   04 Apr 26

#pragma once

#ifndef TQUEUE_TRANSPORT_HPP_INCLUDED
#define TQUEUE_TRANSPORT_HPP_INCLUDED

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
//  TQueue<T>
//  Single Producer, Single Consumer (SPSC) transport
//==============================================================================

template<typename T>
class TQueue
{
private:
    static_assert(!std::is_const_v<T>, "TQueue<T> requires non-const T.");
    static_assert(std::is_trivially_copyable_v<T>, "TQueue<T> requires trivially copyable T.");

public:
    static constexpr std::uint32_t k_max_capacity = 0x00100000u;
    static constexpr std::uint32_t k_min_capacity = static_cast<std::uint32_t>(memory::k_vector_growth_policy_min_capacity);

public:
    TQueue() noexcept = default;
    TQueue(const TQueue&) = delete;
    TQueue& operator=(const TQueue&) = delete;
    TQueue(TQueue&&) noexcept = delete;
    TQueue& operator=(TQueue&&) noexcept = delete;
    ~TQueue() noexcept { (void)deallocate(); }

    //  Producer status
    //  - only safe to call from the producer thread or while quiescent
    [[nodiscard]] bool producer_is_valid() const noexcept;
    [[nodiscard]] bool producer_is_ready() const noexcept;
    [[nodiscard]] bool producer_poisoned() const noexcept;

    //  Consumer status
    //  - only safe to call from the consumer thread or while quiescent
    [[nodiscard]] bool consumer_is_valid() const noexcept;
    [[nodiscard]] bool consumer_is_ready() const noexcept;

    //  Producer operations
    bool post(const T& src) noexcept { return post(&src, 1u); }
    bool post(const T* const src, const std::uint32_t count = 1u) noexcept;
    bool post(const TPodConstView<T>& src) noexcept { return post(src.data(), src.size()); }

    //  Consumer operations
    bool read(T& dst) noexcept { return read(&dst, 1u); }
    bool read(T* const dst, const std::uint32_t count = 1u) noexcept;
    bool read(const TPodView<T>& dst) noexcept { return read(dst.data(), dst.size()); }
    [[nodiscard]] std::uint32_t current_readable_count() const noexcept;
    [[nodiscard]] std::uint32_t refresh_readable_count() noexcept;

    //  Setup and teardown
    //  initialise_*() requires a deallocated / not-ready instance.
    //  deallocate() releases owned storage and must not race active role use.
    [[nodiscard]] bool initialise_fixed(const std::uint32_t capacity, const bool allow_discard = false) noexcept;
    [[nodiscard]] bool initialise_growable(const std::uint32_t capacity, const std::uint32_t max_capacity = 0u) noexcept;
    void deallocate() noexcept;

    //  Validation
    //  - full structural validity check
    //  - only safe to call while quiescent
    bool validate() const noexcept;

private:
    static std::uint32_t growth_policy(const std::uint32_t capacity) noexcept;

    bool is_canonical_empty() const noexcept;

    //  Configuration
    bool m_allow_discard = false;
    std::uint32_t m_capacity = 0u;
    std::uint32_t m_max_capacity = 0u;

    //  Allocation failure poisoning state
    bool m_allocation_failed = false;

    //  Buffer state
    struct Buffer
    {
        memory::TMemoryToken<T> storage;
        std::uint32_t capacity = 0u;
        std::uint32_t size = 0u;
    };

    //  Buffers
    Buffer m_buffers[3];

    //  Producer state - Consumer side must not access this
    bool m_producer_phase = false;
    std::uint32_t m_producer_output_buffer_index = 0u;  //  "owned" by the producer side
    std::uint32_t m_producer_staged_buffer_index = 1u;  //  "unowned" staged
    std::uint32_t m_producer_locked_buffer_index = 2u;  //  "owned" by the consumer side

    //  Consumer state - Producer side must not access this
    bool m_consumer_phase = false;
    std::uint32_t m_consumer_buffer_index = 2u;
    std::uint32_t m_consumer_read_index = 0u;

    //  Publication state
    std::atomic<std::uint32_t> m_staged_word{ 0 };
};

//==============================================================================
//  TQueue<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TQueue<T>::producer_is_valid() const noexcept
{
    if (m_max_capacity == 0u)
    {   //  uninitialised, safe to check canonical empty
        return is_canonical_empty();
    }
    if (m_allocation_failed || (m_capacity > m_max_capacity))
    {
        return false;
    }
    for (std::uint32_t buffer_index = 0u; buffer_index < 3; ++buffer_index)
    {
        const Buffer& buffer = m_buffers[buffer_index];
        if ((buffer.storage.data() == nullptr) || (buffer.capacity < buffer.size) || (buffer.capacity < k_min_capacity) || (buffer.capacity > m_capacity))
        {
            return false;
        }
    }
    if ((m_producer_output_buffer_index > 2u) ||
        (m_producer_staged_buffer_index > 2u) ||
        (m_producer_locked_buffer_index > 2u))
    {
        return false;
    }
    const std::uint32_t permutation_check =
        (1u << m_producer_output_buffer_index) |
        (1u << m_producer_staged_buffer_index) |
        (1u << m_producer_locked_buffer_index);
    if (permutation_check != 7u)
    {
        return false;
    }
    return m_staged_word.load(std::memory_order_relaxed) < 8u;
}

template<typename T>
inline bool TQueue<T>::producer_is_ready() const noexcept
{
    return (!m_allocation_failed) && (m_max_capacity != 0u);
}

template<typename T>
inline bool TQueue<T>::producer_poisoned() const noexcept
{
    return m_allocation_failed;
}

template<typename T>
inline bool TQueue<T>::consumer_is_valid() const noexcept
{
    if (m_max_capacity == 0u)
    {   //  uninitialised, safe to check canonical empty
        return is_canonical_empty();
    }
    if (m_consumer_buffer_index > 2u)
    {
        return false;
    }
    const Buffer& buffer = m_buffers[m_consumer_buffer_index];
    if ((buffer.storage.data() == nullptr) || (buffer.capacity < buffer.size) || (buffer.capacity < k_min_capacity) || (buffer.size < m_consumer_read_index))
    {
        return false;
    }
    return m_staged_word.load(std::memory_order_relaxed) < 8u;
}

template<typename T>
inline bool TQueue<T>::consumer_is_ready() const noexcept
{
    return m_max_capacity != 0u;
}

template<typename T>
inline bool TQueue<T>::post(const T* const src, const std::uint32_t count) noexcept
{
    if (m_allocation_failed || (m_max_capacity == 0u) || (count > m_max_capacity) || ((src == nullptr) && (count != 0u)))
    {
        return false;
    }
    if (count != 0u)
    {
        Buffer& output_buffer = m_buffers[m_producer_output_buffer_index];
        Buffer& staged_buffer = m_buffers[m_producer_staged_buffer_index];
        Buffer& locked_buffer = m_buffers[m_producer_locked_buffer_index];

        //  Growth and buffer discard policy
        bool discard = false;
        const std::uint32_t capacity = std::min(std::max(growth_policy(output_buffer.size + count), m_capacity), m_max_capacity);
        if (count <= (capacity - output_buffer.size))
        {   //  buffer growth can accomodate the new posting
            m_capacity = capacity;
        }
        else if (m_allow_discard)
        {   //  buffer discard and growth can accomodate the new posting
            output_buffer.size = 0u;
            m_capacity = capacity;
            discard = true;
        }
        else
        {   //  buffer discarding is not allowed, discard the current posting
            return false;
        }

        //  Exchange protocol
        const std::size_t byte_count = (static_cast<std::size_t>(count) * sizeof(T));
        if ((output_buffer.capacity != m_capacity) && !output_buffer.storage.reallocate(output_buffer.size, m_capacity))
        {   //  reallocation failed, the buffer is unchanged but the post() operation cannot continue
            m_allocation_failed = true;
            return MV_FAIL_SAFE_ASSERT(false);
        }
        output_buffer.capacity = m_capacity;
        std::memcpy((output_buffer.storage.data() + output_buffer.size), src, byte_count);
        output_buffer.size += count;
        const std::uint32_t received = m_staged_word.exchange((m_producer_output_buffer_index + (m_producer_phase ? 5u : 1u)), std::memory_order_acq_rel);
        if (received == 0u)
        {   //  the staged buffer has been consumed, the just published buffer will be ignored by the consumer, republish rebased buffer
            m_producer_phase = !m_producer_phase;
            if ((locked_buffer.capacity != m_capacity) && !locked_buffer.storage.reallocate(locked_buffer.size, m_capacity))
            {   //  reallocation failed, the buffer is unchanged but the post() operation cannot continue
                m_allocation_failed = true;
                return MV_FAIL_SAFE_ASSERT(false);
            }
            locked_buffer.capacity = m_capacity;
            std::memcpy(output_buffer.storage.data(), src, byte_count);  //  safe to use because the publish of it will be ignored
            std::memcpy(locked_buffer.storage.data(), src, byte_count);  //  safe to use because it has been consumed
            output_buffer.size = locked_buffer.size = count;
            m_staged_word.store((m_producer_locked_buffer_index + (m_producer_phase ? 5u : 1u)), std::memory_order_release);
            const std::uint32_t buffer_index_swap = m_producer_locked_buffer_index;
            m_producer_locked_buffer_index = m_producer_staged_buffer_index;
            m_producer_staged_buffer_index = buffer_index_swap;
        }
        else
        {   //  the staged buffer has not been consumed
            if ((staged_buffer.capacity != m_capacity) && !staged_buffer.storage.reallocate(staged_buffer.size, m_capacity))
            {   //  reallocation failed, the buffer is unchanged but the post() operation cannot continue
                m_allocation_failed = true;
                return MV_FAIL_SAFE_ASSERT(false);
            }
            staged_buffer.capacity = m_capacity;
            if (discard)
            {
                staged_buffer.size = 0u;
            }
            std::memcpy((staged_buffer.storage.data() + staged_buffer.size), src, byte_count);
            staged_buffer.size += count;
            const std::uint32_t buffer_index_swap = m_producer_output_buffer_index;
            m_producer_output_buffer_index = m_producer_staged_buffer_index;
            m_producer_staged_buffer_index = buffer_index_swap;
        }
    }
    return true;
}

template<typename T>
inline bool TQueue<T>::read(T* const dst, const std::uint32_t count) noexcept
{
    if ((m_max_capacity == 0u) || (count > m_max_capacity) || ((dst == nullptr) && (count != 0u)))
    {
        return false;
    }
    if (count != 0u)
    {
        const std::uint32_t available = refresh_readable_count();
        if (count > available)
        {
            return false;
        }
        std::memcpy(dst, (m_buffers[m_consumer_buffer_index].storage.data() + m_consumer_read_index), (static_cast<std::size_t>(count) * sizeof(T)));
        m_consumer_read_index += count;
    }
    return true;
}

template<typename T>
inline std::uint32_t TQueue<T>::current_readable_count() const noexcept
{
    const Buffer& buffer = m_buffers[m_consumer_buffer_index];
    return (buffer.size >= m_consumer_read_index) ? (buffer.size - m_consumer_read_index) : 0u;
}

template<typename T>
inline std::uint32_t TQueue<T>::refresh_readable_count() noexcept
{
    if (m_consumer_read_index == m_buffers[m_consumer_buffer_index].size)
    {
        const std::uint32_t received = m_staged_word.exchange(0u, std::memory_order_acq_rel);
        const std::uint32_t received_id = received & 3u;
        const bool received_phase = (received & 4u) != 0u;
        if ((received_id == 0u) || (received_phase != m_consumer_phase))
        {
            return 0u;
        }
        m_consumer_phase = !m_consumer_phase;
        m_consumer_buffer_index = received_id - 1u;
        m_consumer_read_index = 0u;
    }
    return current_readable_count();
}

template<typename T>
inline bool TQueue<T>::initialise_fixed(const std::uint32_t capacity, const bool allow_discard) noexcept
{
    if (capacity > k_max_capacity)
    {   //  requested capacity not supported
        return false;
    }
    if (m_max_capacity != 0u)
    {   //  re-initialisation is not allowed without deallocation
        return false;
    }
    const std::uint32_t conditioned_capacity = std::min(std::max((bit_ops::is_pow2(capacity) ? capacity : growth_policy(capacity)), k_min_capacity), k_max_capacity);
    m_allow_discard = allow_discard;
    m_capacity = m_max_capacity = conditioned_capacity;
    for (std::uint32_t buffer_index = 0u; buffer_index < 3; ++buffer_index)
    {
        Buffer& buffer = m_buffers[buffer_index];
        if (!buffer.storage.allocate(conditioned_capacity))
        {
            deallocate();
            return false;
        }
        buffer.capacity = conditioned_capacity;
        buffer.size = 0u;
    }
    m_producer_phase = m_consumer_phase = false;
    m_producer_output_buffer_index = 0u;
    m_producer_staged_buffer_index = 1u;
    m_producer_locked_buffer_index = m_consumer_buffer_index = 2u;
    m_consumer_read_index = 0u;
    m_staged_word.store(0u, std::memory_order_release);
    return true;
}

template<typename T>
inline bool TQueue<T>::initialise_growable(const std::uint32_t capacity, const std::uint32_t max_capacity) noexcept
{
    if ((max_capacity > k_max_capacity) || ((capacity > max_capacity) && (max_capacity != 0u)))
    {   //  requested capacity not supported
        return false;
    }
    if (!initialise_fixed(capacity, false))
    {
        return false;
    }
    if (max_capacity != capacity)
    {   //  the user did not specify matching capacities, growth is expected
        m_max_capacity = (max_capacity == 0u) ? k_max_capacity : std::max(max_capacity, m_capacity);
    }
    return true;
}

template<typename T>
inline void TQueue<T>::deallocate() noexcept
{
    m_allow_discard = false;
    m_capacity = m_max_capacity = 0u;
    m_allocation_failed = false;
    for (std::uint32_t buffer_index = 0u; buffer_index < 3; ++buffer_index)
    {
        Buffer& buffer = m_buffers[buffer_index];
        buffer.storage.deallocate();
        buffer.capacity = buffer.size = 0u;
    }
    m_producer_phase = m_consumer_phase = false;
    m_producer_output_buffer_index = 0u;
    m_producer_staged_buffer_index = 1u;
    m_producer_locked_buffer_index = m_consumer_buffer_index = 2u;
    m_consumer_read_index = 0u;
    m_staged_word.store(0u, std::memory_order_relaxed);
}

template<typename T>
inline bool TQueue<T>::validate() const noexcept
{
    return producer_is_valid() && consumer_is_valid() && (m_producer_locked_buffer_index == m_consumer_buffer_index);
}

template<typename T>
inline std::uint32_t TQueue<T>::growth_policy(const std::uint32_t capacity) noexcept
{
    return static_cast<std::uint32_t>(memory::vector_growth_policy(static_cast<std::size_t>(capacity)));
}

template<typename T>
inline bool TQueue<T>::is_canonical_empty() const noexcept
{
    if (m_allocation_failed || (m_max_capacity != 0u))
    {
        return false;
    }
    if ((m_capacity != 0u) || m_allow_discard || m_producer_phase || m_consumer_phase ||
        (m_producer_output_buffer_index != 0u) ||
        (m_producer_staged_buffer_index != 1u) ||
        (m_producer_locked_buffer_index != 2u) ||
        (m_consumer_buffer_index != 2u) ||
        (m_consumer_read_index != 0u))
    {
        return false;
    }
    for (std::uint32_t buffer_index = 0u; buffer_index < 3; ++buffer_index)
    {
        const Buffer& buffer = m_buffers[buffer_index];
        if ((buffer.storage.data() != nullptr) || ((buffer.capacity | buffer.size) != 0u))
        {
            return false;
        }
    }
    return m_staged_word.load(std::memory_order_relaxed) == 0u;
}

}   //  namespace threading::transports

#endif  //  #ifndef TQUEUE_TRANSPORT_HPP_INCLUDED
