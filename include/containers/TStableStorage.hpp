
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TStableStorage.hpp
//  Author: Ritchie Brannan
//  Date:   24 Mar 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Stable segmented storage for typed data over fixed-size buffers.
//
//  Does not track occupancy, construct or destroy T, or interpret
//  stored data.
//
//  IMPORTANT TERMINOLOGY NOTE
//  --------------------------
//  Slot indices map to storage via fixed buffer geometry using
//  bit decomposition.
//
//  Address stability is guaranteed once a slot is backed by storage.
//
//  See docs/containers/TStableStorage.md for the full documentation.

#pragma once

#ifndef TSTABLE_STORAGE_HPP_INCLUDED
#define TSTABLE_STORAGE_HPP_INCLUDED

#include <algorithm>    //  std::max, std::min
#include <cstddef>      //  std::size_t
#include <type_traits>  //  std::is_const_v

#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "bit_utils/bit_ops.hpp"
#include "debug/debug.hpp"

//==============================================================================
//  TStableStorage<T>
//  Owning unique stable raw allocation buffers for typed data.
//==============================================================================

template<typename T>
class TStableStorage
{
private:
    static_assert(!std::is_const_v<T>, "TStableStorage<T> requires non-const T.");

public:

    //  Default and deleted lifetime
    TStableStorage() noexcept = default;
    TStableStorage(const TStableStorage&) noexcept = delete;
    TStableStorage& operator=(const TStableStorage&) noexcept = delete;

    //  Move lifetime
    TStableStorage(TStableStorage&&) noexcept;
    TStableStorage& operator=(TStableStorage&&) noexcept;

    //  Destructor
    ~TStableStorage() noexcept { deallocate(); };

    //  Status
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;

    //  Accessors
    //
    //  - map_index(): allocating, grows storage to cover slot_index.
    //  - index_ptr(): non-allocating, returns nullptr if not backed.
    //  - Both are fail-safe and return nullptr when not ready.
    [[nodiscard]] T* map_index(const std::size_t slot_index) noexcept;
    [[nodiscard]] T* index_ptr(const std::size_t slot_index) const noexcept;

    //  Initialisation and deallocation
    bool initialise(const std::size_t slots_per_buffer) noexcept;
    void deallocate() noexcept;

    //  Constants
    static constexpr std::size_t k_max_elements = memory::t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);

private:
    [[nodiscard]] std::size_t buffer_count() const noexcept { return m_slot_capacity >> m_buffer_shift; }
    [[nodiscard]] std::size_t buffer_slots() const noexcept { return m_slot_mask + 1u; }
    [[nodiscard]] std::size_t buffer_index(const std::size_t slot_index) const noexcept { return slot_index >> m_buffer_shift; }
    [[nodiscard]] std::size_t buffer_slot(const std::size_t slot_index) const noexcept { return slot_index & m_slot_mask; }
    void move_from(TStableStorage& src) noexcept;

    memory::TMemoryToken<memory::TMemoryToken<T>> m_buffers = memory::TMemoryToken<memory::TMemoryToken<T>>{};

    std::size_t m_buffer_capacity = 0u;
    std::size_t m_buffer_shift = 0u;
    std::size_t m_slot_mask = 0u;
    std::size_t m_slot_capacity = 0u;
};

//==============================================================================
//  TStableStorage<T> out of class function bodies
//==============================================================================

template<typename T>
inline TStableStorage<T>::TStableStorage(TStableStorage&& src) noexcept
{
    move_from(src);
}

template<typename T>
inline TStableStorage<T>& TStableStorage<T>::operator=(TStableStorage&& src) noexcept
{
    if (this != &src)
    {
        move_from(src);
    }
    return *this;
}

template<typename T>
inline bool TStableStorage<T>::is_valid() const noexcept
{
    if (m_buffers.data() == nullptr)
    {   //  check valid uninitialised state
        return (m_buffer_capacity | m_buffer_shift | m_slot_mask | m_slot_capacity) == 0u;
    }

    const std::size_t slots_per_buffer = buffer_slots();

    if ((m_buffer_shift >= 5u) &&
        (m_buffer_shift <= bit_ops::hi_bit_index(k_max_elements)) &&
        ((m_slot_mask & slots_per_buffer) == 0u) &&
        ((slots_per_buffer >> m_buffer_shift) == 1u) &&
        bit_ops::is_pow2(m_buffer_capacity) &&
        memory::in_non_empty_range(m_buffer_capacity, (k_max_elements / slots_per_buffer)) &&
        ((m_slot_capacity & m_slot_mask) == 0u) &&
        ((m_slot_capacity >> m_buffer_shift) <= m_buffer_capacity))
    {   //  core invariants hold, now check the buffer pointers
        const memory::TMemoryToken<T>* const buffers = m_buffers.data();
        const std::size_t used_buffer_count = buffer_count();
        std::size_t buffer_index = 0u;
        while (buffer_index < used_buffer_count)
        {   //  check the allocation state of the allocated buffers
            if (buffers[buffer_index].data() == nullptr)
            {   //  an allocated buffer is missing allocation
                return false;
            }
            ++buffer_index;
        }
        while (buffer_index < m_buffer_capacity)
        {   //  check the allocation state of the unnallocated buffers
            if (buffers[buffer_index].data() != nullptr)
            {   //  an unallocated buffer has allocation
                return false;
            }
            ++buffer_index;
        }
        return true;
    }
    return false;
}

template<typename T>
inline bool TStableStorage<T>::is_empty() const noexcept
{
    return !is_ready() || (m_slot_capacity == 0u);
}

template<typename T>
inline bool TStableStorage<T>::is_ready() const noexcept
{
    const std::size_t slots_per_buffer = buffer_slots();

    return
        (m_buffers.data() != nullptr) &&
        bit_ops::is_pow2(m_buffer_capacity) &&
        (m_buffer_shift >= 5u) &&
        (m_buffer_shift <= bit_ops::hi_bit_index(k_max_elements)) &&
        ((m_slot_mask & slots_per_buffer) == 0u) &&
        ((slots_per_buffer >> m_buffer_shift) == 1u) &&
        ((m_slot_capacity & m_slot_mask) == 0u) &&
        ((m_slot_capacity >> m_buffer_shift) <= m_buffer_capacity);
}

template<typename T>
inline T* TStableStorage<T>::map_index(const std::size_t slot_index) noexcept
{
    if (is_ready() && (slot_index < k_max_elements))
    {
        const std::size_t slots_per_buffer = buffer_slots();
        while (slot_index >= m_slot_capacity)
        {   //  allocate additional buffers to fulfil the request
            const std::size_t next_buffer_index = buffer_count();
            if (next_buffer_index == m_buffer_capacity)
            {   //  need to grow the buffer directory
                if (!m_buffers.reallocate(m_buffer_capacity, (m_buffer_capacity << 1u)))
                {   //  reallocation failed
                    return nullptr;
                }
                m_buffer_capacity <<= 1u;
            }
            memory::TMemoryToken<T>* const buffers = m_buffers.data();
            if (buffers == nullptr)
            {
                return nullptr;
            }
            if (!buffers[next_buffer_index].allocate(slots_per_buffer))
            {   //  allocation failed
                return nullptr;
            }
            m_slot_capacity += slots_per_buffer;
        }
        memory::TMemoryToken<T>* const buffers = m_buffers.data();
        if (buffers != nullptr)
        {
            T* buffer = buffers[buffer_index(slot_index)].data();
            if (buffer != nullptr)
            {
                return buffer + buffer_slot(slot_index);
            }
        }
    }
    return nullptr;
}

template<typename T>
inline T* TStableStorage<T>::index_ptr(const std::size_t slot_index) const noexcept
{
    if (is_ready() && (slot_index < m_slot_capacity))
    {
        memory::TMemoryToken<T>* const buffers = m_buffers.data();
        if (buffers != nullptr)
        {
            T* buffer = buffers[buffer_index(slot_index)].data();
            if (buffer != nullptr)
            {
                return buffer + buffer_slot(slot_index);
            }
        }
    }
    return nullptr;
}

template<typename T>
inline bool TStableStorage<T>::initialise(const std::size_t slots_per_buffer) noexcept
{
    if (!is_valid() || (slots_per_buffer > k_max_elements))
    {
        return false;
    }
    const std::size_t pow2_slots_per_buffer = std::max(std::size_t{ 32 }, bit_ops::round_up_to_pow2(slots_per_buffer));
    if (is_ready() && (buffer_slots() == pow2_slots_per_buffer))
    {
        return true;
    }
    deallocate();
    const std::size_t buffer_capacity = std::min<std::size_t>(32u, (k_max_elements / pow2_slots_per_buffer));
    if (m_buffers.allocate(buffer_capacity))
    {
        m_buffer_capacity = buffer_capacity;
        m_buffer_shift = bit_ops::hi_bit_index(pow2_slots_per_buffer);
        m_slot_mask = pow2_slots_per_buffer - 1u;
        m_slot_capacity = 0u;
        return true;
    }
    return false;
}

template<typename T>
inline void TStableStorage<T>::deallocate() noexcept
{
    memory::TMemoryToken<T>* buffers = m_buffers.data();
    if (buffers != nullptr)
    {
        std::size_t count = m_buffer_capacity;
        while (count)
        {
            --count;
            if (buffers[count].data() != nullptr)
            {
                buffers[count].deallocate();
            }
        }
        m_buffers.deallocate();
    }
    m_buffer_capacity = 0u;
    m_buffer_shift = 0u;
    m_slot_mask = 0u;
    m_slot_capacity = 0u;
}

template<typename T>
inline void TStableStorage<T>::move_from(TStableStorage& src) noexcept
{
    m_buffers = std::move(src.m_buffers);
    m_buffer_capacity = src.m_buffer_capacity;
    m_buffer_shift = src.m_buffer_shift;
    m_slot_mask = src.m_slot_mask;
    m_slot_capacity = src.m_slot_capacity;
    src.m_buffer_capacity = 0u;
    src.m_buffer_shift = 0u;
    src.m_slot_mask = 0u;
    src.m_slot_capacity = 0u;
}

#endif  //  TSTABLE_STORAGE_HPP_INCLUDED
