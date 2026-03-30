
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   TOrderedCollection.hpp
//  Author: Ritchie Brannan
//  Date:   24 Mar 26
//
//  Grouped stable address slots for non-trivial types (noexcept allocation substrate)
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//  - Indices, sizes, and capacities are in elements.

#pragma once

#ifndef TORDERED_COLLECTION_HPP_INCLUDED
#define TORDERED_COLLECTION_HPP_INCLUDED

#include <algorithm>    //  std::max, std::min
#include <cstddef>      //  std::size_t
#include <type_traits>  //  std::is_const_v, std::is_trivially_copyable_v

#include "TPodVector.hpp"
#include "TStableStorage.hpp"
#include "slots/TOrderedSlots.hpp"
#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "bit_utils/bit_ops.hpp"
#include "debug/debug.hpp"

//==============================================================================
//  TOrderedCollection<T>
//  Owning unique stable address slot manager for non-trivial types.
//==============================================================================

template<typename T, typename TKey>
class TOrderedCollection : public slots::COrderedSlots_int32
{
private:
    using base_class = slots::COrderedSlots_int32;
    static_assert(!std::is_const_v<T>, "TOrderedCollection<T, TKey> requires non-const T.");
    static_assert(!std::is_const_v<TKey>, "TOrderedCollection<T, TKey> requires non-const TKey.");
    static_assert(std::is_trivially_copyable_v<TKey>, "TOrderedCollection<T, TKey> requires trivially copyable TKey.");

public:

    //  Default and deleted lifetime
    TOrderedCollection() noexcept = default;
    TOrderedCollection(const TOrderedCollection&) noexcept = delete;
    TOrderedCollection& operator=(const TOrderedCollection&) noexcept = delete;
    TOrderedCollection(TOrderedCollection&&) noexcept = default;
    TOrderedCollection& operator=(TOrderedCollection&&) noexcept = default;

    //  Destructor
    ~TOrderedCollection() noexcept { deallocate(); };

    //  Status
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;

    //  Initialisation and deallocation
    bool initialise(const std::size_t initial_slot_count = 0u, const std::size_t slots_per_buffer = 0u) noexcept;
    void deallocate() noexcept;

    //  Constants
    static constexpr std::size_t k_max_elements = memory::t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);

protected:
    virtual void on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept override;
    virtual [[nodiscard]] std::uint32_t on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept override;
    virtual [[nodiscard]] std::int32_t on_compare_keys(const std::int32_t source_index, const std::int32_t target_index) const noexcept override;

private:
    void deconstruct_payload() noexcept;

    enum class SlotState : std::size_t
    {
        Unmapped = 0u,      //  Backing storage has not been mapped to this slot
        Mapped = 1u,        //  Backing storage has been mapped to this slot
        Constructed = 2u    //  Constructed implies both Mapped and Constructed
    };

    struct SlotData
    {
        SlotState state;
        std::size_t storage_index;
    };

    TStableStorage<T> m_storage;
    TPodVector<SlotData> m_slots;
    TPodVector<TKey> m_keys;
    SlotData m_spare_slot;
    TKey m_spare_key;
    TKey m_staged_key;
};

//==============================================================================
//  TOrderedCollection<T> out of class function bodies
//==============================================================================

template<typename T, typename TKey>
inline bool TOrderedCollection<T, TKey>::is_valid() const noexcept
{   //  bare bones - needs hardening pass
    return m_slots.is_valid() && m_storage.is_valid();
}

template<typename T, typename TKey>
inline bool TOrderedCollection<T, TKey>::is_empty() const noexcept
{   //  bare bones - needs hardening pass
    return m_slots.is_empty();
}

template<typename T, typename TKey>
inline bool TOrderedCollection<T, TKey>::is_ready() const noexcept
{   //  bare bones - needs hardening pass
    return m_slots.is_ready() && m_storage.is_ready();
}

template<typename T, typename TKey>
inline bool TOrderedCollection<T, TKey>::initialise(const std::size_t initial_slot_count, const std::size_t slots_per_buffer) noexcept
{
    deallocate();
    if (base_class::initialise(std::max(initial_slot_count, std::size_t{ 32u })))
    {
        if (m_storage.initialise(std::max(slots_per_buffer, std::size_t{ 32u })))
        {
            const std::size_t size = base_class::capacity();
            if (m_slots.initialise(size))
            {
                if (m_keys.initialise(size))
                {
                    for (std::size_t i = 0u; i < size; ++i)
                    {
                        m_slots.push_back({ SlotState::Unmapped, i });
                    }
                    (void)m_keys.set_size(size);
                    return true;
                }
                m_slots.deallocate();
            }
            m_storage.deallocate();
        }
        base_class::shutdown();
    }
    return false;
}

template<typename T, typename TKey>
inline void TOrderedCollection<T, TKey>::deallocate() noexcept
{
    deconstruct_payload();
    base_class::shutdown();
    m_storage.deallocate();
    m_slots.deallocate();
    m_keys.deallocate();
}

template<typename T, typename TKey>
void TOrderedCollection<T, TKey>::on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
{
    SlotData& source_slot = (source_index < 0) ? m_spare_slot : m_slots[source_index];
    SlotData& target_slot = (target_index < 0) ? m_spare_slot : m_slots[target_index];
    target_slot = source_slot;
    TKey& source_key = (source_index < 0) ? m_spare_key : m_keys[source_index];
    TKey& target_key = (target_index < 0) ? m_spare_key : m_keys[target_index];
    target_key = source_key;
}

template<typename T, typename TKey>
std::uint32_t TOrderedCollection<T, TKey>::on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept
{
    (void)minimum_capacity;
    const std::size_t new_capacity = static_cast<std::size_t>(recommended_capacity);
    if (!m_slots.reallocate(new_capacity) || !m_keys.reallocate(new_capacity))
    {   //  decline the reserve attempt and force the base class slot acquisition to fail
        return 0u;
    }
    for (std::size_t i = m_slots.size(); i < new_capacity; ++i)
    {
        (void)m_slots.push_back({ SlotState::Unmapped, i });
    }
    (void)m_keys.set_size(new_capacity);
    return recommended_capacity;
}

template<typename T, typename TKey>
std::int32_t TOrderedCollection<T, TKey>::on_compare_keys(const std::int32_t source_index, const std::int32_t target_index) const noexcept
{
    const TKey& source_key = (source_index < 0) ? m_staged_key : m_keys[source_index];
    const TKey& target_key = (target_index < 0) ? m_staged_key : m_keys[target_index];
    return static_cast<std::int32_t>(source_key.relationship(target_key));
}

template<typename T, typename TKey>
inline void TOrderedCollection<T, TKey>::deconstruct_payload() noexcept
{
    const std::size_t element_count = m_slots.size();
    for (std::size_t element_index = 0u; element_index < element_count; ++element_index)
    {
        SlotData& slot = m_slots[element_index];
        if (slot.state == SlotState::Constructed)
        {
            T* element = m_storage.index_ptr(slot.storage_index);
            if (element != nullptr)
            {
                element->~T();
            }
            slot.state = SlotState::Mapped;
        }
    }
}

#endif  //  TORDERED_COLLECTION_HPP_INCLUDED
