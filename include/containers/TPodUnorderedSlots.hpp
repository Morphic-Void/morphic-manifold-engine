
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TPodUnorderedSlots.hpp
//  Author: Ritchie Brannan
//  Date:   13 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Unordered POD slot manager with slot-based identity.
//
//  Uses TUnorderedSlots for slot management.
//  Stores trivial payload data directly in slot-indexed POD storage.
//
//  IMPORTANT TERMINOLOGY NOTE
//  --------------------------
//  slot_index is the public identity during mutation and is not stable
//  across pack().
//
//  pack() remaps slot metadata and payload data.
//
//  Traversal order does not imply rank or ordering.

#pragma once

#ifndef TPOD_UNORDERED_SLOTS_HPP_INCLUDED
#define TPOD_UNORDERED_SLOTS_HPP_INCLUDED

#include <algorithm>    //  std::max
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::int32_t, std::uint32_t
#include <type_traits>  //  std::is_const_v, std::is_copy_assignable_v, std::is_trivially_copyable_v

#include "memory/memory_allocation.hpp"
#include "slots/TUnorderedSlots.hpp"
#include "slots/SlotsRankMap.hpp"
#include "TPodVector.hpp"

#include "debug/debug.hpp"

//==============================================================================
//  TPodUnorderedSlots<T>
//  Unordered POD slot manager.
//==============================================================================

template<typename T>
class TPodUnorderedSlots : public slots::CUnorderedSlots_int32
{
private:
    using base_class = slots::CUnorderedSlots_int32;

    static_assert(!std::is_const_v<T>, "TPodUnorderedSlots<T> requires non-const T.");
    static_assert(std::is_copy_assignable_v<T>, "TPodUnorderedSlots<T> requires copy-assignable T.");
    static_assert(std::is_trivially_copyable_v<T>, "TPodUnorderedSlots<T> requires trivially copyable T.");

public:

    //  Default and deleted lifetime
    TPodUnorderedSlots() noexcept = default;
    TPodUnorderedSlots(const TPodUnorderedSlots&) noexcept = delete;
    TPodUnorderedSlots& operator=(const TPodUnorderedSlots&) noexcept = delete;
    TPodUnorderedSlots(TPodUnorderedSlots&&) noexcept = default;
    TPodUnorderedSlots& operator=(TPodUnorderedSlots&&) noexcept = default;

    //  Destructor
    ~TPodUnorderedSlots() noexcept { deallocate(); }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;

    //  Accessors
    T* get_slot(const std::int32_t slot_index) noexcept;
    const T* get_slot(const std::int32_t slot_index) const noexcept;

    //  Traversal
    [[nodiscard]] std::int32_t first_live() const noexcept;
    [[nodiscard]] std::int32_t last_live() const noexcept;
    [[nodiscard]] std::int32_t prev_live(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t next_live(const std::int32_t slot_index) const noexcept;

    //  Utility
    [[nodiscard]] slots::RankMap build_rank_map() const noexcept;
    [[nodiscard]] std::int32_t reverse_lookup_index_scan(const T* const slot) const noexcept;

    //  Content management
    std::int32_t insert(const T& value) noexcept;
    bool erase(const std::int32_t slot_index) noexcept;
    void pack() noexcept;

    //  Initialisation and deallocation
    bool initialise(const std::size_t initial_slot_count = 0u) noexcept;
    void deallocate() noexcept;

    //  Integrity audit
    [[nodiscard]] bool check_integrity() const noexcept;

    //  Constants
    static constexpr std::size_t k_max_elements = memory::t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);

protected:
    virtual void on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept override;
    virtual [[nodiscard]] std::uint32_t on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept override;

private:
    static [[nodiscard]] bool failed_integrity_check() noexcept;

    TPodVector<T> m_slots;
};

//==============================================================================
//  TPodUnorderedSlots<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TPodUnorderedSlots<T>::is_valid() const noexcept
{
    return m_slots.is_valid() && (m_slots.size() == base_class::capacity());
}

template<typename T>
inline bool TPodUnorderedSlots<T>::is_empty() const noexcept
{
    return base_class::is_empty();
}

template<typename T>
inline bool TPodUnorderedSlots<T>::is_ready() const noexcept
{
    return m_slots.is_ready();
}

template<typename T>
inline T* TPodUnorderedSlots<T>::get_slot(const std::int32_t slot_index) noexcept
{
    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    if (element_index < m_slots.size())
    {
        if (base_class::is_loose_slot(slot_index))
        {
            return &m_slots[element_index];
        }
    }
    return nullptr;
}

template<typename T>
inline const T* TPodUnorderedSlots<T>::get_slot(const std::int32_t slot_index) const noexcept
{
    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    if (element_index < m_slots.size())
    {
        if (base_class::is_loose_slot(slot_index))
        {
            return &m_slots[element_index];
        }
    }
    return nullptr;
}

template<typename T>
inline std::int32_t TPodUnorderedSlots<T>::first_live() const noexcept
{
    return base_class::first_loose();
}

template<typename T>
inline std::int32_t TPodUnorderedSlots<T>::last_live() const noexcept
{
    return base_class::last_loose();
}

template<typename T>
inline std::int32_t TPodUnorderedSlots<T>::prev_live(const std::int32_t slot_index) const noexcept
{
    return base_class::prev_loose(slot_index);
}

template<typename T>
inline std::int32_t TPodUnorderedSlots<T>::next_live(const std::int32_t slot_index) const noexcept
{
    return base_class::next_loose(slot_index);
}

template<typename T>
inline slots::RankMap TPodUnorderedSlots<T>::build_rank_map() const noexcept
{
    return base_class::build_rank_map();
}

template<typename T>
inline std::int32_t TPodUnorderedSlots<T>::reverse_lookup_index_scan(const T* const slot) const noexcept
{
    const std::size_t element_count = m_slots.size();
    for (std::size_t element_index = 0u; element_index < element_count; ++element_index)
    {
        const std::int32_t slot_index = static_cast<std::int32_t>(element_index);
        if (base_class::is_loose_slot(slot_index))
        {
            if (slot == &m_slots[element_index])
            {
                return slot_index;
            }
        }
    }
    return -1;
}

template<typename T>
inline std::int32_t TPodUnorderedSlots<T>::insert(const T& value) noexcept
{
    const std::int32_t slot_index = base_class::reserve_and_acquire(-1);
    if (slot_index < 0)
    {
        return -1;
    }

    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    m_slots[element_index] = value;
    return slot_index;
}

template<typename T>
inline bool TPodUnorderedSlots<T>::erase(const std::int32_t slot_index) noexcept
{
    return base_class::erase(slot_index);
}

template<typename T>
inline void TPodUnorderedSlots<T>::pack() noexcept
{
    base_class::pack();
}

template<typename T>
inline bool TPodUnorderedSlots<T>::initialise(const std::size_t initial_slot_count) noexcept
{
    deallocate();
    if (base_class::initialise(std::max(static_cast<std::uint32_t>(initial_slot_count), 32u)))
    {
        const std::size_t size = base_class::capacity();
        if (m_slots.allocate(size))
        {
            (void)m_slots.set_size(size);
            return true;
        }
        (void)base_class::shutdown();
    }
    return false;
}

template<typename T>
inline void TPodUnorderedSlots<T>::deallocate() noexcept
{
    (void)::shutdown();
    m_slots.deallocate();
}

template<typename T>
inline bool TPodUnorderedSlots<T>::check_integrity() const noexcept
{
    if (!is_valid())
    {
        return failed_integrity_check();
    }

    if (!base_class::check_integrity())
    {
        return false;
    }

    return true;
}

template<typename T>
inline void TPodUnorderedSlots<T>::on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
{
    T swap = m_slots[static_cast<std::size_t>(target_index)];
    m_slots[static_cast<std::size_t>(target_index)] = m_slots[static_cast<std::size_t>(source_index)];
    m_slots[static_cast<std::size_t>(source_index)] = swap;
}

template<typename T>
inline std::uint32_t TPodUnorderedSlots<T>::on_reserve_empty(
    const std::uint32_t minimum_capacity,
    const std::uint32_t recommended_capacity) noexcept
{
    (void)minimum_capacity;
    const std::size_t new_capacity = static_cast<std::size_t>(recommended_capacity);
    if (!m_slots.reallocate(new_capacity))
    {
        return 0u;
    }
    (void)m_slots.set_size(new_capacity);
    return recommended_capacity;
}

template<typename T>
inline bool TPodUnorderedSlots<T>::failed_integrity_check() noexcept
{
    MV_HARD_ASSERT(false);
    return false;
}

#endif  //  TPOD_UNORDERED_SLOTS_HPP_INCLUDED
