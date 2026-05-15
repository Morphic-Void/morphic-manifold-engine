
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TUnorderedCollection.hpp
//  Author: Ritchie Brannan
//  Date:   24 Mar 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Unordered collection over stable storage with slot-based identity.
//
//  Uses TUnorderedSlots for slot management and TStableStorage for
//  address-stable object backing.
//
//  IMPORTANT TERMINOLOGY NOTE
//  --------------------------
//  slot_index is the public identity during mutation and is not stable
//  across pack().
//
//  pack() remaps slot metadata but does not relocate constructed objects.
//
//  Traversal order does not imply rank or ordering.
//
//  See docs/containers/TUnorderedCollection.md for the full documentation.

#pragma once

#ifndef TUNORDERED_COLLECTION_HPP_INCLUDED
#define TUNORDERED_COLLECTION_HPP_INCLUDED

#include <algorithm>    //  std::max, std::min
#include <cstddef>      //  std::size_t
#include <type_traits>  //  std::is_const_v, std::is_nothrow_destructible_v
#include <utility>      //  std::forward<TArgs>

#include "algo/validate_permutations.hpp"
#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "slots/TUnorderedSlots.hpp"
#include "slots/SlotsRankMap.hpp"
#include "bit_utils/bit_ops.hpp"
#include "TStableStorage.hpp"
#include "TPodVector.hpp"

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
    static_assert(std::is_nothrow_destructible_v<T>, "TUnorderedCollection<T> requires T to be nothrow destructible.");

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

    //  Accessors
    T* get_object(const std::int32_t slot_index) noexcept;
    const T* get_object(const std::int32_t slot_index) const noexcept;

    //  Traversal
    [[nodiscard]] std::int32_t first_live() const noexcept;
    [[nodiscard]] std::int32_t last_live() const noexcept;
    [[nodiscard]] std::int32_t prev_live(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t next_live(const std::int32_t slot_index) const noexcept;

    //  Utility
    [[nodiscard]] slots::RankMap build_rank_map() const noexcept;
    [[nodiscard]] std::int32_t reverse_lookup_slot_index_scan(const T* const object) const noexcept;

    //  Content management
    template<typename... TArgs> std::int32_t emplace(TArgs&&... args) noexcept;
    bool erase(const std::int32_t slot_index) noexcept;
    void pack() noexcept;

    //  Initialisation and deallocation
    bool initialise(const std::size_t initial_slot_count = 0u, const std::size_t slots_per_buffer = 0u) noexcept;
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
    void deconstruct_payload() noexcept;
    static [[nodiscard]] bool failed_integrity_check() noexcept;

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
{
    return
        m_storage.is_valid() &&
        m_slots.is_valid() && (m_slots.size() == base_class::capacity());
}

template<typename T>
inline bool TUnorderedCollection<T>::is_empty() const noexcept
{
    return base_class::is_empty();
}

template<typename T>
inline bool TUnorderedCollection<T>::is_ready() const noexcept
{
    return m_slots.is_ready() && m_storage.is_ready();
}

template<typename T>
inline T* TUnorderedCollection<T>::get_object(const std::int32_t slot_index) noexcept
{
    const std::size_t internal_slot_index = static_cast<std::size_t>(slot_index);
    if (internal_slot_index < m_slots.size())
    {
        const SlotData& slot = m_slots[internal_slot_index];
        if (slot.state == SlotState::Constructed)
        {
            T* const element = m_storage.index_ptr(slot.storage_index);
            MV_HARD_ASSERT(element != nullptr);
            return element;
        }
    }
    return nullptr;
}

template<typename T>
inline const T* TUnorderedCollection<T>::get_object(const std::int32_t slot_index) const noexcept
{
    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    if (element_index < m_slots.size())
    {
        const SlotData& slot = m_slots[element_index];
        if (slot.state == SlotState::Constructed)
        {
            const T* const element = m_storage.index_ptr(slot.storage_index);
            MV_HARD_ASSERT(element != nullptr);
            return element;
        }
    }
    return nullptr;
}

template<typename T>
inline std::int32_t TUnorderedCollection<T>::first_live() const noexcept
{
    return base_class::first_loose();
}

template<typename T>
inline std::int32_t TUnorderedCollection<T>::last_live() const noexcept
{
    return base_class::last_loose();
}

template<typename T>
inline std::int32_t TUnorderedCollection<T>::prev_live(const std::int32_t slot_index) const noexcept
{
    return base_class::prev_loose(slot_index);
}

template<typename T>
inline std::int32_t TUnorderedCollection<T>::next_live(const std::int32_t slot_index) const noexcept
{
    return base_class::next_loose(slot_index);
}

template<typename T>
inline slots::RankMap TUnorderedCollection<T>::build_rank_map() const noexcept
{
    return base_class::build_rank_map();
}

template<typename T>
inline std::int32_t TUnorderedCollection<T>::reverse_lookup_slot_index_scan(const T* const object) const noexcept
{
    const std::size_t element_count = m_slots.size();
    for (std::size_t element_index = 0u; element_index < element_count; ++element_index)
    {
        const SlotData& slot = m_slots[element_index];
        if (slot.state == SlotState::Constructed)
        {
            if (object == m_storage.index_ptr(slot.storage_index))
            {
                return static_cast<std::int32_t>(element_index);
            }
        }
    }
    return -1;
}

template<typename T>
template<typename... TArgs>
inline std::int32_t TUnorderedCollection<T>::emplace(TArgs&&... args) noexcept
{
    //  Acquire a slot index
    const std::int32_t slot_index = base_class::reserve_and_acquire(-1);
    if (slot_index < 0)
    {
        return -1;
    }

    //  Fetch or map backing storage
    T* element = nullptr;
    SlotData& slot = m_slots[static_cast<std::size_t>(slot_index)];
    if (slot.state == SlotState::Unmapped)
    {
        element = m_storage.map_index(slot.storage_index);
        if (element != nullptr)
        {
            slot.state = SlotState::Mapped;
        }
    }
    else if (slot.state == SlotState::Mapped)
    {
        element = m_storage.index_ptr(slot.storage_index);
    }
    if (element == nullptr)
    {
        (void)base_class::erase(slot_index);
        MV_HARD_ASSERT(false);
        return -1;
    }

    //  Construct the object
    new (element) T(std::forward<TArgs>(args)...);
    slot.state = SlotState::Constructed;
    return slot_index;
}

template<typename T>
inline bool TUnorderedCollection<T>::erase(const std::int32_t slot_index) noexcept
{
    const std::size_t element_index = static_cast<std::size_t>(slot_index);
    if (element_index < m_slots.size())
    {
        SlotData& slot = m_slots[element_index];
        if (slot.state == SlotState::Constructed)
        {
            T* const element = m_storage.index_ptr(slot.storage_index);
            MV_HARD_ASSERT(element != nullptr);
            if (element != nullptr)
            {
                element->~T();
                slot.state = SlotState::Mapped;
                return base_class::erase(slot_index);
            }
        }
    }
    return false;
}

template<typename T>
inline void TUnorderedCollection<T>::pack() noexcept
{
    base_class::pack();
}

template<typename T>
inline bool TUnorderedCollection<T>::initialise(const std::size_t initial_slot_count, const std::size_t slots_per_buffer) noexcept
{
    deallocate();
    if (base_class::initialise(std::max(static_cast<std::uint32_t>(initial_slot_count), 32u)))
    {
        if (m_storage.initialise(std::max(slots_per_buffer, std::size_t{ 32u })))
        {
            const std::size_t size = base_class::capacity();
            if (m_slots.allocate(size))
            {
                for (std::size_t i = 0u; i < size; ++i)
                {
                    (void)m_slots.push_back({ SlotState::Unmapped, i });
                }
                return true;
            }
            m_storage.deallocate();
        }
        (void)base_class::shutdown();
    }
    return false;
}

template<typename T>
inline void TUnorderedCollection<T>::deallocate() noexcept
{
    deconstruct_payload();
    (void)base_class::shutdown();
    m_storage.deallocate();
    m_slots.deallocate();
}

template<typename T>
inline bool TUnorderedCollection<T>::check_integrity() const noexcept
{
    //  basic structural integrity check
    if (!is_valid())
    {
        return failed_integrity_check();
    }

    //  base class integrity check
    if (!base_class::check_integrity())
    {   //  no need to catch the error here as the base class will have already caught it
        return false;
    }

    //  metadata coherence check
    const std::size_t element_count = m_slots.size();
    for (std::size_t element_index = 0u; element_index < element_count; ++element_index)
    {
        const std::int32_t slot_index = static_cast<std::int32_t>(element_index);
        const SlotData& slot = m_slots[element_index];
        if (slot.storage_index >= element_count)
        {
            return failed_integrity_check();
        }
        switch (slot.state)
        {
            case(SlotState::Unmapped):
            {
                if (!base_class::is_empty_slot(slot_index))
                {
                    return failed_integrity_check();
                }
                break;
            }
            case(SlotState::Mapped):
            {
                if (!base_class::is_empty_slot(slot_index))
                {
                    return failed_integrity_check();
                }
                if (m_storage.index_ptr(slot.storage_index) == nullptr)
                {
                    return failed_integrity_check();
                }
                break;
            }
            case(SlotState::Constructed):
            {
                if (!base_class::is_loose_slot(slot_index))
                {
                    return failed_integrity_check();
                }
                if (m_storage.index_ptr(slot.storage_index) == nullptr)
                {
                    return failed_integrity_check();
                }
                break;
            }
            default:
            {
                return failed_integrity_check();
            }
        }
    }

    //  storage mapping permutation check
    if (!algo::validate_extracted_permutation(m_slots.data(), m_slots.size(),
        [](const SlotData& slot) noexcept { return slot.storage_index; }))
    {
        return failed_integrity_check();
    }

    return true;
}

template<typename T>
inline void TUnorderedCollection<T>::on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
{
    SlotData swap = m_slots[target_index];
    m_slots[target_index] = m_slots[source_index];
    m_slots[source_index] = swap;
}

template<typename T>
inline std::uint32_t TUnorderedCollection<T>::on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept
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
            MV_HARD_ASSERT(element != nullptr);
            if (element != nullptr)
            {
                element->~T();
                slot.state = SlotState::Mapped;
                (void)base_class::erase(static_cast<std::int32_t>(element_index));
            }
        }
    }
}

template<typename T>
inline bool TUnorderedCollection<T>::failed_integrity_check() noexcept
{
    MV_HARD_ASSERT(false);
    return false;
}

#endif  //  TUNORDERED_COLLECTION_HPP_INCLUDED
