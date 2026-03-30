
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   TUnorderedCollection.hpp
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

#ifndef TUNORDERED_COLLECTION_HPP_INCLUDED
#define TUNORDERED_COLLECTION_HPP_INCLUDED

#include <algorithm>    //  std::max, std::min
#include <cstddef>      //  std::size_t
#include <type_traits>  //  std::is_const_v

#include "TPodVector.hpp"
#include "TStableStorage.hpp"
#include "slots/TUnorderedSlots.hpp"
#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "bit_utils/bit_ops.hpp"
#include "debug/debug.hpp"

//==============================================================================
//  TUnorderedCollection<T>
//  Owning unique stable address slot manager for non-trivial types.
//==============================================================================

template<typename T>
class TUnorderedCollection : public slots::CUnorderedSlots_int32
{
private:
    using base_class = slots::CUnorderedSlots_int32;
    static_assert(!std::is_const_v<T>, "TUnorderedCollection<T> requires non-const T.");

public:

    //  Default and deleted lifetime
    TUnorderedCollection() noexcept = default;
    TUnorderedCollection(const TUnorderedCollection&) noexcept = delete;
    TUnorderedCollection& operator=(const TUnorderedCollection&) noexcept = delete;
    TUnorderedCollection(TUnorderedCollection&&) noexcept = default;
    TUnorderedCollection& operator=(TUnorderedCollection&&) noexcept = default;

    //  Destructor
    ~TUnorderedCollection() noexcept { deallocate(); };

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
};

//==============================================================================
//  TUnorderedCollection<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TUnorderedCollection<T>::is_valid() const noexcept
{   //  bare bones - needs hardening pass
    return m_slots.is_valid() && m_storage.is_valid();
}

template<typename T>
inline bool TUnorderedCollection<T>::is_empty() const noexcept
{   //  bare bones - needs hardening pass
    return m_slots.is_empty();
}

template<typename T>
inline bool TUnorderedCollection<T>::is_ready() const noexcept
{   //  bare bones - needs hardening pass
    return m_slots.is_ready() && m_storage.is_ready();
}

template<typename T>
inline bool TUnorderedCollection<T>::initialise(const std::size_t initial_slot_count, const std::size_t slots_per_buffer) noexcept
{
    deallocate();
    if (base_class::initialise(std::max(initial_slot_count, std::size_t{ 32u })))
    {
        if (m_storage.initialise(std::max(slots_per_buffer, std::size_t{ 32u })))
        {
            const std::size_t size = base_class::capacity();
            if (m_slots.initialise(size))
            {
                for (std::size_t i = 0u; i < size; ++i)
                {
                    m_slots.push_back({ SlotState::Unmapped, i });
                }
                return true;
            }
            m_storage.deallocate();
        }
        base_class::shutdown();
    }
    return false;
}

template<typename T>
inline void TUnorderedCollection<T>::deallocate() noexcept
{
    deconstruct_payload();
    base_class::shutdown();
    m_storage.deallocate();
    m_slots.deallocate();
}

template<typename T>
void TUnorderedCollection<T>::on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
{
    SlotData swap = m_slots[target_index];
    m_slots[target_index] = m_slots[source_index];
    m_slots[source_index] = swap;
}

template<typename T>
std::uint32_t TUnorderedCollection<T>::on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept
{
    (void)minimum_capacity;
    const std::size_t new_capacity = static_cast<std::size_t>(recommended_capacity);
    if (!m_slots.reallocate(new_capacity))
    {   //  decline the reserve attempt and force the base class slot acquisition to fail
        return 0u;
    }
    for (std::size_t i = m_slots.size(); i < new_capacity; ++i)
    {
        (void)m_slots.push_back({ SlotState::Unmapped, i });
    }
    return recommended_capacity;
}

template<typename T>
inline void TUnorderedCollection<T>::deconstruct_payload() noexcept
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

#endif  //  TUNORDERED_COLLECTION_HPP_INCLUDED
