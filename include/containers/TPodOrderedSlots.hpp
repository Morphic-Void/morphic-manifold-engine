
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TPodOrderedSlots.hpp
//  Author: Ritchie Brannan
//  Date:   13 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Ordered POD slot manager with slot-based identity and key-based ordering.
//
//  Uses TOrderedSlots for ordering and slot management.
//  Stores trivial payload data directly in slot-indexed POD storage.
//
//  IMPORTANT TERMINOLOGY NOTE
//  --------------------------
//  slot_index is the public identity during mutation and is not stable
//  across sort_and_pack().
//
//  sort_and_pack() remaps slot metadata, payload data, and keys.
//
//  Ordered traversal is defined over live keyed slots.

#pragma once

#ifndef TPOD_ORDERED_SLOTS_HPP_INCLUDED
#define TPOD_ORDERED_SLOTS_HPP_INCLUDED

#include <algorithm>    //  std::max
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::int32_t, std::uint32_t
#include <type_traits>  //  std::is_const_v, std::is_trivially_copyable_v

#include "memory/memory_allocation.hpp"
#include "slots/TOrderedSlots.hpp"
#include "slots/SlotsRankMap.hpp"
#include "TPodVector.hpp"

#include "debug/debug.hpp"

//==============================================================================
//  TPodOrderedSlots<T, TKey>
//  Ordered keyed POD slot manager.
//==============================================================================

template<typename T, typename TKey>
class TPodOrderedSlots : public slots::COrderedSlots_int32
{
private:
    using base_class = slots::COrderedSlots_int32;

    static_assert(!std::is_const_v<T>, "TPodOrderedSlots<T, TKey> requires non-const T.");
    static_assert(!std::is_const_v<TKey>, "TPodOrderedSlots<T, TKey> requires non-const TKey.");
    static_assert(std::is_trivially_copyable_v<T>, "TPodOrderedSlots<T, TKey> requires trivially copyable T.");
    static_assert(std::is_trivially_copyable_v<TKey>, "TPodOrderedSlots<T, TKey> requires trivially copyable TKey.");

public:

    //  Default and deleted lifetime
    TPodOrderedSlots() noexcept = default;
    TPodOrderedSlots(const TPodOrderedSlots&) noexcept = delete;
    TPodOrderedSlots& operator=(const TPodOrderedSlots&) noexcept = delete;
    TPodOrderedSlots(TPodOrderedSlots&&) noexcept = default;
    TPodOrderedSlots& operator=(TPodOrderedSlots&&) noexcept = default;

    //  Destructor
    ~TPodOrderedSlots() noexcept { deallocate(); }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;

    //  Accessors
    T* get_slot(const TKey& key) noexcept;
    T* get_slot(const std::int32_t slot_index) noexcept;
    const T* get_slot(const TKey& key) const noexcept;
    const T* get_slot(const std::int32_t slot_index) const noexcept;

    //  Traversal
    [[nodiscard]] std::int32_t first_live() const noexcept;
    [[nodiscard]] std::int32_t last_live() const noexcept;
    [[nodiscard]] std::int32_t prev_live(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t next_live(const std::int32_t slot_index) const noexcept;

    //  Utility
    [[nodiscard]] slots::RankMap build_rank_map() const noexcept;
    [[nodiscard]] std::int32_t reverse_lookup_index_scan(const T* const slot) const noexcept;
    [[nodiscard]] std::int32_t find_index(const TKey& key) const noexcept;

    //  Content management
    std::int32_t insert(const TKey& key, const T& value) noexcept;
    bool erase(const TKey& key) noexcept;
    bool erase(const std::int32_t slot_index) noexcept;
    void sort_and_pack() noexcept;

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
    virtual [[nodiscard]] std::int32_t on_compare_keys(const std::int32_t source_index, const std::int32_t target_index) const noexcept override;

private:
    static [[nodiscard]] bool failed_integrity_check() noexcept;

    TPodVector<T> m_slots;
    TPodVector<TKey> m_keys;
    T m_swap_slot;
    TKey m_swap_key;

    mutable TKey m_staged_key;
};

//==============================================================================
//  TPodOrderedSlots<T, TKey> out of class function bodies
//==============================================================================

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::is_valid() const noexcept
{
    return
        m_slots.is_valid() && (m_slots.size() == base_class::capacity()) &&
        m_keys.is_valid() && (m_keys.size() == base_class::capacity());
}

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::is_empty() const noexcept
{
    return base_class::is_empty();
}

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::is_ready() const noexcept
{
    return m_slots.is_ready() && m_keys.is_ready();
}

template<typename T, typename TKey>
inline T* TPodOrderedSlots<T, TKey>::get_slot(const TKey& key) noexcept
{
    m_staged_key = key;
    return get_slot(base_class::find_any_equal());
}

template<typename T, typename TKey>
inline T* TPodOrderedSlots<T, TKey>::get_slot(const std::int32_t slot_index) noexcept
{
    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    if (element_index < m_slots.size())
    {
        if (base_class::is_lexed_slot(slot_index))
        {
            return &m_slots[element_index];
        }
    }
    return nullptr;
}

template<typename T, typename TKey>
inline const T* TPodOrderedSlots<T, TKey>::get_slot(const TKey& key) const noexcept
{
    m_staged_key = key;
    return get_slot(base_class::find_any_equal());
}

template<typename T, typename TKey>
inline const T* TPodOrderedSlots<T, TKey>::get_slot(const std::int32_t slot_index) const noexcept
{
    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    if (element_index < m_slots.size())
    {
        if (base_class::is_lexed_slot(slot_index))
        {
            return &m_slots[element_index];
        }
    }
    return nullptr;
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::find_index(const TKey& key) const noexcept
{
    m_staged_key = key;
    return base_class::find_any_equal();
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::first_live() const noexcept
{
    return base_class::first_lexed();
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::last_live() const noexcept
{
    return base_class::last_lexed();
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::prev_live(const std::int32_t slot_index) const noexcept
{
    return base_class::prev_lexed(slot_index);
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::next_live(const std::int32_t slot_index) const noexcept
{
    return base_class::next_lexed(slot_index);
}

template<typename T, typename TKey>
inline slots::RankMap TPodOrderedSlots<T, TKey>::build_rank_map() const noexcept
{
    return base_class::build_rank_map();
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::reverse_lookup_index_scan(const T* const slot) const noexcept
{
    const std::size_t element_count = m_slots.size();
    for (std::size_t element_index = 0u; element_index < element_count; ++element_index)
    {
        const std::int32_t slot_index = static_cast<std::int32_t>(element_index);
        if (base_class::is_lexed_slot(slot_index))
        {
            if (slot == &m_slots[element_index])
            {
                return slot_index;
            }
        }
    }
    return -1;
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::insert(const TKey& key, const T& value) noexcept
{
    m_staged_key = key;
    const std::int32_t slot_index = base_class::reserve_and_acquire(-1, /* lex */ true, /* require_unique */ true);
    if (slot_index < 0)
    {
        return -1;
    }

    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    m_slots[element_index] = value;
    m_keys[element_index] = key;
    return slot_index;
}

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::erase(const TKey& key) noexcept
{
    return erase(find_index(key));
}

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::erase(const std::int32_t slot_index) noexcept
{
    return base_class::erase(slot_index);
}

template<typename T, typename TKey>
inline void TPodOrderedSlots<T, TKey>::sort_and_pack() noexcept
{
    base_class::sort_and_pack(false);
}

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::initialise(const std::size_t initial_slot_count) noexcept
{
    deallocate();
    if (base_class::initialise(std::max(initial_slot_count, std::size_t{ 32u })))
    {
        const std::size_t size = base_class::capacity();
        if (m_slots.initialise(size))
        {
            if (m_keys.initialise(size))
            {
                (void)m_slots.set_size(size);
                (void)m_keys.set_size(size);
                return true;
            }
            m_slots.deallocate();
        }
        base_class::shutdown();
    }
    return false;
}

template<typename T, typename TKey>
inline void TPodOrderedSlots<T, TKey>::deallocate() noexcept
{
    base_class::shutdown();
    m_slots.deallocate();
    m_keys.deallocate();
}

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::check_integrity() const noexcept
{
    if (!is_valid())
    {
        return failed_integrity_check();
    }

    if (!base_class::check_integrity())
    {
        return false;
    }

    return base_class::validate_tree(base_class::LexCheck::Unique);
}

template<typename T, typename TKey>
inline void TPodOrderedSlots<T, TKey>::on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
{
    T& source_slot = (source_index < 0) ? m_swap_slot : m_slots[static_cast<std::size_t>(source_index)];
    T& target_slot = (target_index < 0) ? m_swap_slot : m_slots[static_cast<std::size_t>(target_index)];
    target_slot = source_slot;

    TKey& source_key = (source_index < 0) ? m_swap_key : m_keys[static_cast<std::size_t>(source_index)];
    TKey& target_key = (target_index < 0) ? m_swap_key : m_keys[static_cast<std::size_t>(target_index)];
    target_key = source_key;
}

template<typename T, typename TKey>
inline std::uint32_t TPodOrderedSlots<T, TKey>::on_reserve_empty(
    const std::uint32_t minimum_capacity,
    const std::uint32_t recommended_capacity) noexcept
{
    (void)minimum_capacity;
    const std::size_t new_capacity = static_cast<std::size_t>(recommended_capacity);
    if (!m_slots.reallocate(new_capacity) || !m_keys.reallocate(new_capacity))
    {
        return 0u;
    }
    (void)m_slots.set_size(new_capacity);
    (void)m_keys.set_size(new_capacity);
    return recommended_capacity;
}

template<typename T, typename TKey>
inline std::int32_t TPodOrderedSlots<T, TKey>::on_compare_keys(
    const std::int32_t source_index,
    const std::int32_t target_index) const noexcept
{
    const TKey& source_key = (source_index < 0) ? m_staged_key : m_keys[static_cast<std::size_t>(source_index)];
    const TKey& target_key = (target_index < 0) ? m_staged_key : m_keys[static_cast<std::size_t>(target_index)];
    return static_cast<std::int32_t>(source_key.relationship(target_key));
}

template<typename T, typename TKey>
inline bool TPodOrderedSlots<T, TKey>::failed_integrity_check() noexcept
{
    MV_HARD_ASSERT(false);
    return false;
}

#endif  //  TPOD_ORDERED_SLOTS_HPP_INCLUDED
