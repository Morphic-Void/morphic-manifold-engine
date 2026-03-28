
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   TUnorderedSlots.hpp
//  Author: Ritchie Brannan
//  Date:   10 Jan 26
//
// 
//  IMPORTANT SEMANTIC NOTE:
//
//  In TUnorderedSlots, TRAVERSAL ORDER IS LIST ORDER.
// 
//  Traversal order does NOT define rank order.
//
//  on_visit() traverses in list order only. No rank_index is
//  supplied or implied during traversal.
//
//  After pack(), the loose list is rebuilt in ascending slot
//  index order, and the packed position is equal to slot_index.
//
//  Prior to pack(), no dense rank ordering is implied.
//
//  Do not assume that TOrderedSlots rank semantics apply here.
// 
// 
//  TUnorderedSlots<TIndex>
//
//  Overview
//  --------
//  TUnorderedSlots is a single-threaded unordered index
//  over an external slot array.
//
//  The template owns and manages only slot metadata:
//      - Bi-directional list links (prev_index, next_index)
//      - Slot state (loose / empty / unassigned / terminator)
//      - Category counts and high/peak tracking
//
//  The derived class owns the payload and must implement:
//
//      on_visit(slot_index)
//      on_move_payload(source_index, target_index)
//      on_reserve_empty(minimum_capacity, recommended_capacity)
//
//  Slot Model
//  ----------
//  Slot indices are integers in [0, capacity()). Each index addresses both
//  slot metadata and its corresponding derived payload element.
//
//  Slot categories:
//      Loose  - acquired/occupied
//      Empty  - available for acquisition
//
//  Steady-State Invariants
//  -----------------------
//  - m_loose_count + m_empty_count == m_capacity
//  - Loose and empty slots form circular bi-directional lists
//  - m_high_index is the highest occupied slot index (or -1 if none)
//  - m_peak_usage and m_peak_index record historical maxima
//  - No slot belongs to more than one category
//
//  Lifecycle
//  ---------
//  - initialise(capacity) allocates metadata and marks all slots Empty
//  - shutdown() releases metadata and resets to uninitialised
//  - safe_resize()/reserve_empty() adjust capacity subject to invariants
//  - pack() physically moves payload removing gaps and rebuilds metadata
//
//  Locking and Re-Entry Model
//  --------------------------
//  The template is strictly single-threaded.
//
//  During execution of a virtual callback the template enters a lock state:
//
//      LockState::on_visit
//      LockState::on_move_payload
//      LockState::on_reserve_empty
//
//  While locked:
//      - Only explicitly safe accessor functions may be called
//      - All other protected functions are unsafe
//      - Debug: unsafe calls hard-fail
//      - Release: unsafe calls soft-fail (return false / -1, no mutation)
//
//  Functions safe during virtual callbacks:
//
//      is_initialised(), capacity(), capacity_limit(), minimum_safe_capacity()
//      peak_usage(), peak_index(), high_index(), index_limit()
//      loose_count(), empty_count()
//
//  These functions are non-mutating, do not acquire locks,
//  do not invoke virtual functions, and do not call is_safe().
//
//  Integrity and Validation
//  ------------------------
//  - check_integrity() validates metadata invariants, counts, list structure, and index ranges.
//
//  Capacity Constraints
//  --------------------
//  - Maximum supported slot index is std::numeric_limits<TIndex>::max()
//  - capacity_limit() == index_limit() + 1
//  - minimum_safe_capacity() == (m_high_index + 1)
//
//  Type Constraints
//  ----------------
//  Supported types:
//      std::int32_t
//      std::int16_t
//
//  Slot metadata layout is constrained for predictable packing.
//  Slot must be trivially copyable.

#pragma once

#ifndef TUNORDERED_SLOTS_HPP_INCLUDED
#define TUNORDERED_SLOTS_HPP_INCLUDED

#include <algorithm>    //  std::fill_n, std::min
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::int16_t, std::int32_t, std::uint32_t, std::uintptr_t
#include <cstring>      //  std::memcpy
#include <limits>       //  std::numeric_limits
#include <type_traits>  //  std::is_trivially_copyable_v, std::is_signed_v, std::is_same_v

#include "TPodVector.hpp"
#include "memory/memory_allocation.hpp"
#include "debug/debug.hpp"

/// Unordered index over slot metadata for externally stored payload items.
///
/// TUnorderedSlots stores only slot metadata (list links and occupancy state).
/// The derived class owns the payload items and defines how payload items are
/// moved between slots.
///
/// See docs/TUnorderedSlots.md for full terminology and usage patterns.
template<typename TIndex = std::int32_t>
class TUnorderedSlots
{
public:
    TUnorderedSlots() noexcept = default;
    TUnorderedSlots(TUnorderedSlots&& src) noexcept { set_empty(); (void)move_from(src); }
    TUnorderedSlots(const TUnorderedSlots& src) noexcept { set_empty(); (void)copy_from(src); }
    TUnorderedSlots(const std::uint32_t capacity) noexcept { (void)initialise(capacity); }
    virtual ~TUnorderedSlots() noexcept { (void)shutdown(); }

public:

    //  The derived class is expected to provide the public interface.

protected:

    //  Protected functions represent the derived class facing interface.

private:

    //  Private functions are implementation details of the template.

protected:

    //  These functions are safe to call during virtual calls from this template.

    [[nodiscard]] bool is_initialised() const noexcept;

    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t minimum_safe_capacity() const noexcept;

    [[nodiscard]] std::uint32_t peak_usage() const noexcept;
    [[nodiscard]] std::int32_t  peak_index() const noexcept;
    [[nodiscard]] std::int32_t  high_index() const noexcept;

    [[nodiscard]] std::uint32_t loose_count() const noexcept;
    [[nodiscard]] std::uint32_t empty_count() const noexcept;

    [[nodiscard]] static constexpr std::uint32_t index_limit() { return k_index_limit; }
    [[nodiscard]] static constexpr std::uint32_t capacity_limit() { return k_capacity_limit; }

protected:

    //  These functions are unsafe to call during virtual calls from this template.

    TUnorderedSlots& operator=(TUnorderedSlots&& src) noexcept;
    TUnorderedSlots& operator=(const TUnorderedSlots& src) noexcept;

    [[nodiscard]] bool take(TUnorderedSlots& src) noexcept;
    [[nodiscard]] bool clone(const TUnorderedSlots& src) noexcept;

    //  Reset the management data returning all slots to the empty list.
    [[nodiscard]] bool clear() noexcept;

    //  Deallocate all management data and then set defaults.
    [[nodiscard]] bool shutdown() noexcept;

    //  Initialise or re-initialise.
    //  Calls shutdown() then allocates and initialises all management data.
    [[nodiscard]] bool initialise(const std::uint32_t capacity = 32) noexcept;

    //  Capacity management functions.
    [[nodiscard]] bool safe_resize(const std::uint32_t requested_capacity) noexcept;
    [[nodiscard]] bool reserve_empty(const std::uint32_t slot_count) noexcept;
    [[nodiscard]] bool shrink_to_fit() noexcept;

    //  Acquire an empty slot.
    //  If slot_index is -1, acquires the first slot from the empty list.
    //  Returns the acquired slot index, or -1 on failure.
    [[nodiscard]] std::int32_t acquire(const std::int32_t slot_index = -1) noexcept;

    //  Reserve space to acquire the requested slot and then acquire it.
    //  Returns the acquired slot index, or -1 on failure.
    [[nodiscard]] std::int32_t reserve_and_acquire(const std::int32_t slot_index = -1) noexcept;

    //  Assign a slot to the empty list removing it from loose list.
    //  The derived class is responsible for handling or discarding the payload item(s).
    //  Returns false if the slot was invalid or empty.
    bool erase(const std::int32_t slot_index) noexcept;

    //  Slot categorisation functions.
    [[nodiscard]] bool is_loose_slot(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool is_empty_slot(const std::int32_t slot_index) const noexcept;

    //  Traversal of loose payload items using slot indices.
    [[nodiscard]] std::int32_t first_loose() const noexcept;
    [[nodiscard]] std::int32_t last_loose() const noexcept;
    [[nodiscard]] std::int32_t prev_loose(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t next_loose(const std::int32_t slot_index) const noexcept;

    //  Physically reorder payload items.
    //
    //  After completion:
    //      - Loose payload items occupy slot indices [0, loose_count()).
    //      - All remaining slots are Empty.
    //
    //  Uses on_move_payload().
    void pack() noexcept;

    //  Index order list rebuilding utilities.
    //  For loose slots index order == rank_index order.
    void rebuild_loose_in_index_order() noexcept;
    void rebuild_empty_in_index_order() noexcept;

    //  Build slot_index -> rank_index mapping.
    //  The rank represents the index of the slot payload after sort_and_pack().
    //  Loose slots have ranks in the range [0, loose_count()).
    //  Empty slots have a rank of -1 (empty index order is indeterminate).
    //  The returned vector may be empty, indicating failure or no data.
    [[nodiscard]] TPodVector<std::int32_t> build_rank_indices() const noexcept;

    //  Build slot_index -> rank_index mapping.
    //  The out_rank_indices array must have capacity() or more entries.
    [[nodiscard]] bool build_rank_indices(std::int32_t* const out_rank_indices) const noexcept;

    //  Return the ranked index of a payload item by slot index, or -1 if the slot is empty.
    [[nodiscard]] std::int32_t rank_index_of(const std::int32_t slot_index) const noexcept;

    //  Return the slot index of a payload item by its ranked index, or -1 if the ranked index out of range.
    [[nodiscard]] std::int32_t find_by_rank_index(const std::int32_t rank_index) const noexcept;

    //  Visit one or more slot categories.
    //
    //  For each visited slot, calls on_visit(slot_index, identifier).
    //  The identifier is category-derived and is not a slot or rank index:
    //
    //      - identifier == -1 for loose slots
    //      - identifier == -2 for empty slots
    void visit_loose() noexcept;
    void visit_empty() noexcept;
    void visit_all() noexcept;

    //  Integrity check.
    [[nodiscard]] bool check_integrity() const noexcept;

protected:

    //  Protected virtual functions represent the derived class responsibility interface.
    //
    //  The following functions are safe to call during these virtual function calls:
    //
    //      is_initialised(),
    //      capacity(), capacity_limit(), minimum_safe_capacity(),
    //      peak_usage(), peak_index(), high_index(), index_limit(),
    //      loose_count(), empty_count()
    //
    //  All other functions are unsafe when called from these virtual functions,
    //  and calling them will result in a soft-fail (hard-fail in debug).

    /// Visit callback for category traversal.
    ///
    /// identifier is category-derived and is not a slot index:
    ///
    ///     - identifier == -1 for loose slots
    ///     - identifier == -2 for empty slots
    virtual void on_visit(const std::int32_t slot_index, const std::int32_t identifier) noexcept
    {
        (void)slot_index;
    }

    /// Move a payload item between slots.
    ///
    /// Contract:
    ///   - source_index != target_index always
    ///   - exactly one of {source_index, target_index} may be -1
    ///   - -1 indicates temporary storage owned by the derived class
    ///
    /// This function is only called by pack().
    virtual void on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
    {
        (void)source_index;
        (void)target_index;
    }

    /// Handshake with the derived class to approve and finalise reserve_empty() or reserve_and_acquire() growth.
    /// Return an absolute capacity to apply; returned capacities < min_required_capacity will cause the caller to soft fail.
    virtual [[nodiscard]] std::uint32_t on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept
    {
        (void)minimum_capacity;
        return recommended_capacity;
    }

private:

    //  Private virtual call re-entry guard structures and functions.

    enum class LockState : std::uint32_t { none = 0, on_visit, on_move_payload, on_reserve_empty };

    inline [[nodiscard]] bool is_safe(const bool allow_null = false) const noexcept;
    inline [[nodiscard]] bool lock(const LockState lock, const bool allow_null = false) const noexcept;
    inline void unlock(const LockState unlock) const noexcept;

    //  Private lock protected virtual call wrappers.
    //  safe_on_visit() computes identifier internally from the visited category.
    void safe_on_visit(const std::int32_t slot_index) noexcept;
    void safe_on_visit_dispatcher(const bool visit_loose, const bool visit_empty) noexcept;
    void safe_on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept;
    [[nodiscard]] std::uint32_t safe_on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept;

private:

    //  Private slot metadata structures.

    enum class SlotState : std::uint32_t
    {
        is_unassigned = 0u,
        is_empty_slot = 1u,
        is_loose_slot = 2u,
        is_terminator = 3u
    };

    struct Slot
    {   //  Slot meta data structure
    private:
        static const std::uint32_t k_mask = static_cast<std::uint32_t>(std::numeric_limits<TIndex>::max());
        static const std::uint32_t k_flag = k_mask + 1u;

        TIndex prev_index, next_index;

    public:
        inline void set_prev_index(const std::int32_t index) noexcept { prev_index = static_cast<TIndex>((static_cast<std::uint32_t>(prev_index) & k_flag) | (static_cast<std::uint32_t>(index) & k_mask)); }
        inline void set_next_index(const std::int32_t index) noexcept { next_index = static_cast<TIndex>((static_cast<std::uint32_t>(next_index) & k_flag) | (static_cast<std::uint32_t>(index) & k_mask)); }

        constexpr std::int32_t get_prev_index() const noexcept { return static_cast<std::int32_t>(static_cast<std::uint32_t>(prev_index) & k_mask); }
        constexpr std::int32_t get_next_index() const noexcept { return static_cast<std::int32_t>(static_cast<std::uint32_t>(next_index) & k_mask); }

        inline void set_slot_state(const SlotState slot_state) noexcept {
            prev_index = static_cast<TIndex>((static_cast<std::uint32_t>(prev_index) & k_mask) | ((static_cast<std::uint32_t>(slot_state) & 1) ? k_flag : 0));
            next_index = static_cast<TIndex>((static_cast<std::uint32_t>(next_index) & k_mask) | ((static_cast<std::uint32_t>(slot_state) & 2) ? k_flag : 0)); }

        inline void set_is_unassigned() noexcept { set_slot_state(SlotState::is_unassigned); }
        inline void set_is_empty_slot() noexcept { set_slot_state(SlotState::is_empty_slot); }
        inline void set_is_loose_slot() noexcept { set_slot_state(SlotState::is_loose_slot); }
        inline void set_is_terminator() noexcept { set_slot_state(SlotState::is_terminator); }

        constexpr SlotState get_slot_state() const noexcept {
            return static_cast<SlotState>(
                ((static_cast<std::uint32_t>(prev_index) & k_flag) ? 1 : 0) |
                ((static_cast<std::uint32_t>(next_index) & k_flag) ? 2 : 0)); }

        constexpr bool is_unassigned() const noexcept { return get_slot_state() == SlotState::is_unassigned; }
        constexpr bool is_empty_slot() const noexcept { return get_slot_state() == SlotState::is_empty_slot; }
        constexpr bool is_loose_slot() const noexcept { return get_slot_state() == SlotState::is_loose_slot; }
        constexpr bool is_terminator() const noexcept { return get_slot_state() == SlotState::is_terminator; }

        constexpr bool is_invalid() const noexcept {
            SlotState state = get_slot_state();
            return (state != SlotState::is_loose_slot) && (state != SlotState::is_empty_slot); }
    };

private:

    //  Private implementation functions.

    //  Capacity growth recommendation.
    static inline std::uint32_t apply_growth_policy(const std::uint32_t capacity) noexcept;

    //  Build slot_index -> rank_index table.
    //  Ranks follow the canonical traversal order:
    //  - Loose slots next, ranks [0, loose_count()).
    //  Empty slots have rank -1.
    void build_rank_index_table(std::int32_t* const out_rank_indices) const noexcept;

    //  Integrity check functions.
    static inline bool failed_integrity_check() noexcept;
    [[nodiscard]] bool private_integrity_check() const noexcept;

    //  Dispatcher for batched on-visit calls.
    void private_on_visit_dispatcher(const bool visit_loose, const bool visit_empty) noexcept;

    //  Resize the array capacity and apply any resulting required cleanup.
    [[nodiscard]] bool private_resize(const std::uint32_t requested_capacity) noexcept;

    //  Acquire an empty slot.
    //  Optionally reserve space to acquire the requested slot.
    //  If slot_index is -1, acquires the first slot from the empty list.
    //  Returns the acquired slot index, or -1 on failure.
    [[nodiscard]] std::int32_t private_acquire(const std::int32_t slot_index, const bool allow_reserve) noexcept;

    //  Implementation of pack().
    //  See the pack() function prefix comments for details.
    void private_compact() noexcept;

    //  Scan the inclusive range of slot indices and create a a bi-directional index ordered list of the specified category members.
    //  The caller is responsible for correcting any orphaned list members.
    //  Returns the index of the list head.
    [[nodiscard]] std::int32_t state_to_list(const std::int32_t lower_index, const std::int32_t upper_index, const SlotState state) noexcept;

    //  Convert an inclusive range of slot indices to a bi-directional list.
    //  The caller must ensure these slots are not currently actively managed.
    //  Returns the index of the list head.
    [[nodiscard]] std::int32_t range_to_list(const std::int32_t lower_index, const std::int32_t upper_index, const SlotState state) noexcept;

    //  Combine bi-directional lists by inserting list 2 at the end of list 1.
    //  Returns the index of the list head.
    [[nodiscard]] std::int32_t combine_lists(const std::int32_t list1_head_index, const std::int32_t list2_head_index) noexcept;

    //  Set the prev_index of slots in a list to be an ordinal index
    void set_list_ordinals(const std::int32_t list_index, const std::uint32_t list_count, const std::int32_t ordinal_start) noexcept;

    //  Append a range of slot indices to the loose or empty list.
    //  The caller must ensure these slots are not currently actively managed.
    void append_range_to_loose_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept;
    void append_range_to_empty_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept;

    //  Internal helper functions for the move_to_* functions.
    void attach_to_loose(const std::int32_t slot_index) noexcept;
    void attach_to_empty(const std::int32_t slot_index) noexcept;
    void remove_from_loose(const std::int32_t slot_index) noexcept;
    void remove_from_empty(const std::int32_t slot_index) noexcept;

    //  Move a meta slot to a new meta category if not already a member of it.
    void move_to_loose_list(const std::int32_t slot_index) noexcept;
    void move_to_empty_list(const std::int32_t slot_index) noexcept;

    //  Convert a slot index to its rank index.
    //  Returns the rank index for lexed and loose slots, or -1 if the slot is empty.
    [[nodiscard]] std::int32_t convert_to_rank_index(const std::int32_t slot_index) const noexcept;

    //  Locate a slot index by its rank index.
    //  Returns the slot index or -1 if rank_index out of range.
    //  Valid ranks are in [0, lexed_count() + loose_count()).
    [[nodiscard]] std::int32_t locate_by_rank_index(const std::int32_t rank_index) const noexcept;

    //  Scan for lowest/highest occupied slot index in the slot metadata array.
    [[nodiscard]] std::int32_t min_occupied_index() const noexcept;
    [[nodiscard]] std::int32_t max_occupied_index() const noexcept;

    //  These functions should only be called on construction or after a call to shutdown().
    bool move_from(TUnorderedSlots& src) noexcept;
    bool copy_from(const TUnorderedSlots& src) noexcept;
    void set_empty() noexcept;

private:

    //  Private data.
    std::uint32_t m_capacity = 0u;              //  allocated slot count
    std::uint32_t m_peak_usage = 0u;            //  peak occupied slot count
    std::int32_t  m_peak_index = -1;            //  peak occupied slot index
    std::int32_t  m_high_index = -1;            //  highest currently occupied index
    std::uint32_t m_loose_count = 0u;           //  count of slots in the loose list
    std::uint32_t m_empty_count = 0u;           //  count of slots in the empty list
    std::int32_t  m_loose_list_head = -1;       //  index of the loose slot list head (or -1)
    std::int32_t  m_empty_list_head = -1;       //  index of the empty slot list head (or -1)
    Slot*         m_meta_slot_array = nullptr;  //  slot meta data array

    //  Private lock state.
    mutable LockState m_lock = LockState::none;

    //  The maximum supported index is the highest positive signed value. *** DO NOT INCREASE THIS ***
    static constexpr std::int32_t k_index_limit = static_cast<std::int32_t>(std::numeric_limits<TIndex>::max());

    //  The maximum supported capacity is the maximum supported index + 1. *** DO NOT INCREASE THIS ***
    static constexpr std::uint32_t k_capacity_limit = static_cast<std::uint32_t>(k_index_limit) + 1u;

private:

    //  Private static_assert section.

    //  Verify that Slot is trivially copyable (it should be, so just defending against compiler variants)
    static_assert(std::is_trivially_copyable_v<Slot>,
        "TUnorderedSlots: Slot must be trivially copyable.");

    //  Enforce std::size_t has at least 32 bits
    static_assert(sizeof(std::size_t) >= sizeof(std::uint32_t),
        "TUnorderedSlots: std::size_t must be at least 32 bits.");

    //  Enforce signed integer types so that negative sentinels and sign-based comparisons behave correctly
    static_assert(std::is_signed_v<TIndex>,
        "TUnorderedSlots: TIndex must be a signed integer type.");

    //  Enforce the only supported type pairs
    static_assert(std::is_same_v<TIndex, std::int32_t> || std::is_same_v<TIndex, std::int16_t>,
        "TUnorderedSlots: Supported type are std::int32_t and std::int16_t.");

};

//! Protected function bodies

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::is_initialised() const noexcept
{
    return m_meta_slot_array != nullptr;
}

template<typename TIndex>
inline std::uint32_t TUnorderedSlots<TIndex>::capacity() const noexcept
{
    return m_capacity;
}

template<typename TIndex>
inline std::uint32_t TUnorderedSlots<TIndex>::minimum_safe_capacity() const noexcept
{
    return static_cast<std::uint32_t>(m_high_index) + 1u;
}

template<typename TIndex>
inline std::uint32_t TUnorderedSlots<TIndex>::peak_usage() const noexcept
{
    return m_peak_usage;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::peak_index() const noexcept
{
    return m_peak_index;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::high_index() const noexcept
{
    return m_high_index;
}

template<typename TIndex>
inline std::uint32_t TUnorderedSlots<TIndex>::loose_count() const noexcept
{
    return m_loose_count;
}

template<typename TIndex>
inline std::uint32_t TUnorderedSlots<TIndex>::empty_count() const noexcept
{
    return m_empty_count;
}

template<typename TIndex>
inline TUnorderedSlots<TIndex>& TUnorderedSlots<TIndex>::operator=(TUnorderedSlots&& src) noexcept
{
    (void)take(src);
    return *this;
}

template<typename TIndex>
inline TUnorderedSlots<TIndex>& TUnorderedSlots<TIndex>::operator=(const TUnorderedSlots& src) noexcept
{
    (void)clone(src);
    return *this;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::take(TUnorderedSlots& src) noexcept
{
    return shutdown() ? move_from(src) : false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::clone(const TUnorderedSlots& src) noexcept
{
    return shutdown() ? copy_from(src) : false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::clear() noexcept
{
    if (is_safe())
    {
        m_peak_usage = 0;
        m_peak_index = -1;
        m_high_index = -1;
        m_loose_count = 0;
        m_empty_count = m_capacity;
        m_loose_list_head = -1;
        m_empty_list_head = range_to_list(0, static_cast<std::int32_t>(m_empty_count - 1), SlotState::is_empty_slot);
        m_lock = LockState::none;
        return true;
    }
    return false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::shutdown() noexcept
{
    if (is_safe(true))
    {
        if (m_meta_slot_array != nullptr)
        {
            memory::t_deallocate<Slot>(m_meta_slot_array);
        }
        set_empty();
        return true;
    }
    return false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::initialise(const std::uint32_t capacity) noexcept
{
    return shutdown() ? private_resize(capacity) : false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::safe_resize(const std::uint32_t requested_capacity) noexcept
{
    return is_safe(true) ? private_resize(requested_capacity) : false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::reserve_empty(const std::uint32_t slot_count) noexcept
{
    bool reserved = false;
    if (is_safe(true))
    {
        if (m_empty_count >= slot_count)
        {
            reserved = true;
        }
        else
        {
            std::uint32_t slot_limit = k_capacity_limit - m_loose_count;
            if (slot_limit >= slot_count)
            {
                std::uint32_t minimum_capacity = m_loose_count + slot_count;
                std::uint32_t reserve_capacity = safe_on_reserve_empty(minimum_capacity, apply_growth_policy(minimum_capacity));
                if (reserve_capacity >= minimum_capacity)
                {
                    reserved = private_resize(reserve_capacity);
                }
            }
        }
    }
    return reserved;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::shrink_to_fit() noexcept
{
    return (is_safe() && (m_high_index >= 0)) ? private_resize(static_cast<std::uint32_t>(m_high_index) + 1u) : false;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::acquire(const std::int32_t slot_index) noexcept
{
    return is_safe() ? private_acquire(slot_index, false) : -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::reserve_and_acquire(const std::int32_t slot_index) noexcept
{
    return is_safe() ? private_acquire(slot_index, true) : -1;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::erase(const std::int32_t slot_index) noexcept
{
    if (is_loose_slot(slot_index))
    {
        move_to_empty_list(slot_index);
        if (m_high_index == slot_index)
        {
            if (m_loose_count == 0)
            {
                m_high_index = -1;
            }
            else
            {
                for (--m_high_index; !m_meta_slot_array[m_high_index].is_loose_slot(); --m_high_index) {}
            }
        }
        return true;
    }
    return false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::is_loose_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe()
        && (static_cast<std::uint32_t>(slot_index) < m_capacity)
        && m_meta_slot_array[slot_index].is_loose_slot();
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::is_empty_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe()
        && (static_cast<std::uint32_t>(slot_index) < m_capacity)
        && m_meta_slot_array[slot_index].is_empty_slot();
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::first_loose() const noexcept
{
    return is_safe() ? m_loose_list_head : -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::last_loose() const noexcept
{
    return (is_safe() && (m_loose_list_head != -1)) ? m_meta_slot_array[m_loose_list_head].get_prev_index() : -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::prev_loose(const std::int32_t slot_index) const noexcept
{
    return is_loose_slot(slot_index) ? m_meta_slot_array[slot_index].get_prev_index() : -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::next_loose(const std::int32_t slot_index) const noexcept
{
    return is_loose_slot(slot_index) ? m_meta_slot_array[slot_index].get_next_index() : -1;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::pack() noexcept
{
    if (lock(LockState::on_move_payload))
    {
        private_compact();
        unlock(LockState::on_move_payload);
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::rebuild_loose_in_index_order() noexcept
{
    if (is_safe() && (m_loose_count != 0))
    {
        m_loose_list_head = state_to_list(0, m_high_index, SlotState::is_loose_slot);
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::rebuild_empty_in_index_order() noexcept
{
    if (is_safe() && (m_empty_count != 0))
    {
        m_empty_list_head = combine_lists(
            state_to_list(0, m_high_index, SlotState::is_empty_slot),
            range_to_list((m_high_index + 1), static_cast<std::int32_t>(m_capacity - 1u), SlotState::is_empty_slot));
    }
}

template<typename TIndex>
inline TPodVector<std::int32_t> TUnorderedSlots<TIndex>::build_rank_indices() const noexcept
{
    TPodVector<std::int32_t> rank_indices;
    if (is_safe())
    {
        if (rank_indices.allocate(static_cast<std::size_t>(m_capacity)))
        {
            build_rank_index_table(rank_indices.data());
        }
    }
    return rank_indices;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::build_rank_indices(std::int32_t* const out_rank_indices) const noexcept
{
    if (is_safe() && (out_rank_indices != nullptr))
    {
        build_rank_index_table(out_rank_indices);
        return true;
    }
    return false;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::rank_index_of(const std::int32_t slot_index) const noexcept
{
    return is_safe() ? convert_to_rank_index(slot_index) : -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::find_by_rank_index(const std::int32_t rank_index) const noexcept
{
    return is_safe() ? locate_by_rank_index(rank_index) : -1;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::visit_loose() noexcept
{
    safe_on_visit_dispatcher(true, false);
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::visit_empty() noexcept
{
    safe_on_visit_dispatcher(false, true);
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::visit_all() noexcept
{
    safe_on_visit_dispatcher(true, true);
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::check_integrity() const noexcept
{
    return is_safe() ? private_integrity_check() : false;
}

//! Private function bodies

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::is_safe(const bool allow_null) const noexcept
{
    return MV_FAIL_SAFE_ASSERT(m_lock == LockState::none) && (allow_null || (m_meta_slot_array != nullptr));
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::lock(const LockState lock, const bool allow_null) const noexcept
{
    bool success = false;
    if (MV_FAIL_SAFE_ASSERT(m_lock == LockState::none) && (allow_null || (m_meta_slot_array != nullptr)))
    {
        m_lock = lock;
        success = true;
    }
    return success;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::unlock(const LockState unlock) const noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_lock == unlock))
    {
        m_lock = LockState::none;
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::safe_on_visit(const std::int32_t slot_index) noexcept
{
    if (lock(LockState::on_visit))
    {
        std::int32_t identifier = 0;
        switch (m_meta_slot_array[slot_index].get_slot_state())
        {
            case (SlotState::is_loose_slot):
            {
                identifier = -1;
                break;
            }
            case (SlotState::is_empty_slot):
            {
                identifier = -2;
                break;
            }
            default:
            {   //  corruption detected
                MV_HARD_ASSERT(false);
                identifier = -3;
                break;
            }
        }
        on_visit(slot_index, identifier);
        unlock(LockState::on_visit);
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::safe_on_visit_dispatcher(const bool visit_loose, const bool visit_empty) noexcept
{
    if (lock(LockState::on_visit))
    {
        private_on_visit_dispatcher(visit_loose, visit_empty);
        unlock(LockState::on_visit);
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::safe_on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
{
    if (lock(LockState::on_move_payload))
    {
        on_move_payload(source_index, target_index);
        unlock(LockState::on_move_payload);
    }
}

template<typename TIndex>
inline std::uint32_t TUnorderedSlots<TIndex>::safe_on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept
{
    std::uint32_t reserve_capacity = 0u;
    if (lock(LockState::on_reserve_empty))
    {
        reserve_capacity = on_reserve_empty(minimum_capacity, recommended_capacity);
        unlock(LockState::on_reserve_empty);
    }
    return reserve_capacity;
}

template<typename TIndex>
inline std::uint32_t TUnorderedSlots<TIndex>::apply_growth_policy(const std::uint32_t capacity) noexcept
{
    return static_cast<std::uint32_t>(std::min(memory::vector_growth_policy(static_cast<std::size_t>(capacity)), static_cast<std::size_t>(k_capacity_limit)));
}

//  Build slot_index -> rank_index mapping.
template<typename TIndex>
inline void TUnorderedSlots<TIndex>::build_rank_index_table(std::int32_t* const out_rank_indices) const noexcept
{
    std::fill_n(out_rank_indices, m_capacity, -1);
    if (m_loose_count != 0)
    {
        std::int32_t rank_index = 0;
        std::int32_t scan_index = 0;
        for (std::uint32_t count = static_cast<std::uint32_t>(m_high_index) + 1u; count != 0; --count)
        {
            if (m_meta_slot_array[scan_index].is_loose_slot())
            {
                out_rank_indices[scan_index] = rank_index;
                ++rank_index;
            }
            ++scan_index;
        }
    }
}

//  This function only exists as a debug convenience to help capture integrity check failure causes.
//  It may be expanded on in the future as a potential logging site.
template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::failed_integrity_check() noexcept
{
    MV_HARD_ASSERT(false);
    return false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::private_integrity_check() const noexcept
{
    if (m_meta_slot_array != nullptr)
    {
        if ((std::uintptr_t(m_meta_slot_array) % alignof(Slot)) != 0u)
        {   //  basic slot array alignment check failed
            return failed_integrity_check();
        }

        if ((m_capacity == 0) || (m_capacity > k_capacity_limit) ||
            ((m_loose_count + m_empty_count) != m_capacity) ||
            (m_loose_count > m_capacity) || (m_empty_count > m_capacity))
        {   //  basic capacity integrity test failed
            return failed_integrity_check();
        }

        if (((static_cast<std::uint32_t>(m_loose_list_head) + 1u) > m_capacity) || ((m_loose_count == 0u) ? (m_loose_list_head != -1) : (m_loose_list_head == -1)))
        {   //  basic loose list head index integrity test failed
            return failed_integrity_check();
        }

        if (((static_cast<std::uint32_t>(m_empty_list_head) + 1u) > m_capacity) || ((m_empty_count == 0u) ? (m_empty_list_head != -1) : (m_empty_list_head == -1)))
        {   //  basic empty list head index integrity test failed
            return failed_integrity_check();
        }

        if ((m_high_index < -1) || ((m_high_index == -1) ? (m_loose_count != 0u) : ((static_cast<std::uint32_t>(m_high_index) + 1u) < m_loose_count)))
        {   //  basic high index integrity test failed
            return failed_integrity_check();
        }

        if ((m_peak_index < -1) || (m_peak_usage > k_capacity_limit) || (m_peak_usage > (static_cast<std::uint32_t>(m_peak_index) + 1u)))
        {   //  basic peak usage and peak index integrity test failed
            return failed_integrity_check();
        }

        std::uint32_t loose_count = 0;
        std::uint32_t empty_count = 0;
        for (std::int32_t slot_index = static_cast<std::int32_t>(m_capacity - 1u); slot_index >= 0; --slot_index)
        {   //  basic array integrity check
            const Slot& slot = m_meta_slot_array[slot_index];
            SlotState state = slot.get_slot_state();
            if ((state == SlotState::is_loose_slot) || (state == SlotState::is_empty_slot))
            {
                if (state == SlotState::is_loose_slot)
                {
                    ++loose_count;
                }
                else
                {
                    ++empty_count;
                }
                if ((static_cast<std::uint32_t>(slot.get_prev_index()) >= m_capacity) || (static_cast<std::uint32_t>(slot.get_next_index()) >= m_capacity))
                {   //  loose or empty list index is invalid
                    return failed_integrity_check();
                }
            }
            else
            {   //  slot state is invalid
                return failed_integrity_check();
            }
        }
        if (loose_count != m_loose_count)
        {   //  the loose count is invalid
            return failed_integrity_check();
        }
        if (empty_count != m_empty_count)
        {   //  the empty count is invalid
            return failed_integrity_check();
        }
        if (empty_count != 0)
        {   //  validate the empty list
            std::int32_t empty_index = m_empty_list_head;
            while (empty_count != 0)
            {
                const Slot& slot = m_meta_slot_array[empty_index];
                if (!slot.is_empty_slot())
                {   //  the empty list links to a non-empty slot
                    return failed_integrity_check();
                }
                if (m_meta_slot_array[slot.get_next_index()].get_prev_index() != empty_index)
                {   //  bi-directional linkage is broken
                    return failed_integrity_check();
                }
                empty_index = slot.get_next_index();
                if ((empty_index == m_empty_list_head) && (empty_count != 1))
                {   //  found a short cycle
                    return failed_integrity_check();
                }
                --empty_count;
            }
            if (empty_index != m_empty_list_head)
            {   //  the empty list is not circular
                return failed_integrity_check();
            }
        }
        if (loose_count != 0)
        {   //  validate the loose list
            std::int32_t loose_index = m_loose_list_head;
            while (loose_count != 0)
            {
                const Slot& slot = m_meta_slot_array[loose_index];
                if (!slot.is_loose_slot())
                {   //  the loose list links to a non-loose slot
                    return failed_integrity_check();
                }
                if (m_meta_slot_array[slot.get_next_index()].get_prev_index() != loose_index)
                {   //  bi-directional linkage is broken
                    return failed_integrity_check();
                }
                loose_index = slot.get_next_index();
                if ((loose_index == m_loose_list_head) && (loose_count != 1))
                {   //  found a short cycle
                    return failed_integrity_check();
                }
                --loose_count;
            }
            if (loose_index != m_loose_list_head)
            {   //  the loose list is not circular
                return failed_integrity_check();
            }
        }
    }
    else
    {
        if ((m_capacity != 0) || (m_peak_usage != 0) ||
            (m_peak_index != -1) || (m_high_index != -1) ||
            (m_loose_count != 0) || (m_loose_list_head != -1) ||
            (m_empty_count != 0) || (m_empty_list_head != -1))
        {
            return failed_integrity_check();
        }
    }
    return true;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::private_on_visit_dispatcher(const bool visit_loose, const bool visit_empty) noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_visit);
    if (visit_loose)
    {
        std::int32_t slot_index = m_loose_list_head;
        for (std::uint32_t loose_count = m_loose_count; loose_count != 0; --loose_count)
        {
            on_visit(slot_index, -1);
            slot_index = m_meta_slot_array[slot_index].get_next_index();
        }
    }
    if (visit_empty)
    {
        std::int32_t slot_index = m_empty_list_head;
        for (std::uint32_t empty_count = m_empty_count; empty_count != 0; --empty_count)
        {
            on_visit(slot_index, -2);
            slot_index = m_meta_slot_array[slot_index].get_next_index();
        }
    }
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::private_resize(const std::uint32_t requested_capacity) noexcept
{
    bool resized = false;
    if ((requested_capacity != 0) && (requested_capacity <= k_capacity_limit))
    {
        if (requested_capacity == m_capacity)
        {
            resized = true;
        }
        else if (m_meta_slot_array == nullptr)
        {   //  initialisation
            m_meta_slot_array = memory::t_allocate<Slot>(static_cast<std::size_t>(requested_capacity));
            if (m_meta_slot_array != nullptr)
            {
                m_capacity = m_empty_count = requested_capacity;
                m_empty_list_head = range_to_list(0, static_cast<std::int32_t>(m_empty_count - 1), SlotState::is_empty_slot);
                resized = true;
            }
        }
        else if (requested_capacity >= minimum_safe_capacity())
        {
            Slot* new_meta_slot_array = memory::t_allocate<Slot>(static_cast<std::size_t>(requested_capacity));
            if (new_meta_slot_array != nullptr)
            {
                std::memcpy(new_meta_slot_array, m_meta_slot_array, (sizeof(Slot) * std::min(requested_capacity, m_capacity)));
                memory::t_deallocate<Slot>(m_meta_slot_array);
                m_meta_slot_array = new_meta_slot_array;
                if (requested_capacity > m_capacity)
                {   //  grow
                    m_empty_list_head = combine_lists(m_empty_list_head, range_to_list(static_cast<std::int32_t>(m_capacity), static_cast<std::int32_t>(requested_capacity - 1), SlotState::is_empty_slot));
                    m_empty_count += requested_capacity - m_capacity;
                    m_capacity = requested_capacity;
                }
                else
                {   //  shrink
                    m_capacity = requested_capacity;
                    m_empty_count = m_capacity - m_loose_count;
                    m_empty_list_head = (m_empty_count != 0) ? state_to_list(0, static_cast<std::int32_t>(m_capacity - 1u), SlotState::is_empty_slot) : -1;
                }
                resized = true;
            }
        }
    }
    return resized;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::private_acquire(const std::int32_t slot_index, const bool allow_reserve) noexcept
{
    std::int32_t acquired_index = -1;
    if ((static_cast<std::uint32_t>(slot_index) + 1u) <= k_capacity_limit)
    {
        if (slot_index == -1)
        {
            acquired_index = m_empty_list_head;
            if (allow_reserve && (acquired_index == -1) && (m_capacity < k_capacity_limit))
            {   //  need to reserve
                std::uint32_t minimum_capacity = m_capacity + 1u;
                std::uint32_t reserve_capacity = safe_on_reserve_empty(minimum_capacity, apply_growth_policy(minimum_capacity));
                if (reserve_capacity >= minimum_capacity)
                {
                    if (private_resize(reserve_capacity))
                    {
                        acquired_index = m_empty_list_head;
                    }
                }
            }
        }
        else if (static_cast<std::uint32_t>(slot_index) >= m_capacity)
        {   //  need to reserve
            if (allow_reserve)
            {
                std::uint32_t minimum_capacity = static_cast<std::uint32_t>(slot_index) + 1u;
                std::uint32_t reserve_capacity = safe_on_reserve_empty(minimum_capacity, minimum_capacity);
                if (reserve_capacity >= minimum_capacity)
                {
                    if (private_resize(reserve_capacity))
                    {
                        acquired_index = slot_index;
                    }
                }
            }
        }
        else if ((static_cast<std::uint32_t>(slot_index) < m_capacity) && m_meta_slot_array[slot_index].is_empty_slot())
        {
            acquired_index = slot_index;
        }
        if (acquired_index >= 0)
        {
            move_to_loose_list(acquired_index);
            if (m_peak_usage < m_loose_count)
            {
                m_peak_usage = m_loose_count;
            }
            if (m_high_index < acquired_index)
            {
                m_high_index = acquired_index;
                if (m_peak_index < acquired_index)
                {
                    m_peak_index = acquired_index;
                }
            }
        }
    }
    return acquired_index;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::private_compact() noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_move_payload);
    if (m_loose_count)
    {
        std::int32_t target_index = 0;
        std::uint32_t count = m_loose_count;
        for (std::int32_t source_index = 0; count != 0; ++source_index)
        {
            if (m_meta_slot_array[source_index].is_loose_slot())
            {
                if (source_index != target_index)
                {
                    on_move_payload(source_index, target_index);
                }
                ++target_index;
                --count;
            }
        }
    
        const std::int32_t loose_lower_index = 0;
        const std::int32_t loose_upper_index = loose_lower_index + static_cast<std::int32_t>(m_loose_count - 1);
    
        const std::int32_t empty_lower_index = loose_lower_index + static_cast<std::int32_t>(m_loose_count);
        const std::int32_t empty_upper_index = empty_lower_index + static_cast<std::int32_t>(m_empty_count - 1);
    
        m_loose_list_head = range_to_list(loose_lower_index, loose_upper_index, SlotState::is_loose_slot);
        m_empty_list_head = range_to_list(empty_lower_index, empty_upper_index, SlotState::is_empty_slot);
    
        m_high_index = static_cast<std::int32_t>(m_loose_count - 1u);
    }
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::state_to_list(const std::int32_t lower_index, const std::int32_t upper_index, const SlotState state) noexcept
{
    std::int32_t head_index = -1;
    if ((lower_index <= upper_index) && (lower_index >= 0))
    {
        std::int32_t prev_index = -1;
        std::int32_t next_index = -1;
        std::uint32_t scan_count = 0;
        for (std::int32_t scan_index = upper_index; scan_index >= lower_index; --scan_index)
        {   //  scan backwards creating a singly linked forward list
            Slot& slot = m_meta_slot_array[scan_index];
            if (slot.get_slot_state() == state)
            {   //  note: the first encountered slot sets the next index to a silently invalid value
                slot.set_next_index(next_index);
                next_index = scan_index;
                ++scan_count;
            }
        }
        head_index = next_index;
        if (head_index != -1)
        {   //  note: after this loop the next index will contain the silently invalid index
            while (scan_count != 0)
            {   //  scan the singly linked list patching it up to a bi-directional list
                Slot& slot = m_meta_slot_array[next_index];
                slot.set_prev_index(prev_index);
                prev_index = next_index;
                next_index = slot.get_next_index();
                --scan_count;
            }

            //  fix up the list to be circular (next_index is silently invalid at this point)
            m_meta_slot_array[head_index].set_prev_index(prev_index);
            m_meta_slot_array[prev_index].set_next_index(head_index);
        }
    }
    return head_index;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::range_to_list(const std::int32_t lower_index, const std::int32_t upper_index, SlotState state) noexcept
{
    if ((lower_index <= upper_index) && (lower_index >= 0))
    {
        for (std::int32_t scan_index = lower_index; scan_index <= upper_index; ++scan_index)
        {   //  scan the range creating new list members
            Slot& slot = m_meta_slot_array[scan_index];
            slot.set_prev_index(scan_index - 1);    //  potentially transiently invalid
            slot.set_next_index(scan_index + 1);    //  potentially transiently invalid
            slot.set_slot_state(state);
        }

        //  fix up the list to be circular (and correct any transiently invalid index values)
        m_meta_slot_array[upper_index].set_next_index(lower_index);
        m_meta_slot_array[lower_index].set_prev_index(upper_index);
        return lower_index;
    }
    return -1;
}

//  Convert a range of slot indices to a bi-directional list (slot metadata only).
template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::combine_lists(const std::int32_t list1_head_index, const std::int32_t list2_head_index) noexcept
{
    if (list1_head_index < 0)
    {
        return list2_head_index;
    }
    if (list2_head_index >= 0)
    {
        std::int32_t list1_tail_index = m_meta_slot_array[list1_head_index].get_prev_index();
        std::int32_t list2_tail_index = m_meta_slot_array[list2_head_index].get_prev_index();
        m_meta_slot_array[list1_head_index].set_prev_index(list2_tail_index);
        m_meta_slot_array[list1_tail_index].set_next_index(list2_head_index);
        m_meta_slot_array[list2_head_index].set_prev_index(list1_tail_index);
        m_meta_slot_array[list2_tail_index].set_next_index(list1_head_index);
    }
    return list1_head_index;
}

//  Set the prev_index of slots in a list to be an ordinal index
template<typename TIndex>
inline void TUnorderedSlots<TIndex>::set_list_ordinals(const std::int32_t list_index, const std::uint32_t list_count, const std::int32_t ordinal_start) noexcept
{
    std::int32_t ordinal_index = ordinal_start;
    std::int32_t slot_index = list_index;
    for (std::uint32_t slot_count = list_count; slot_count > 0; --slot_count)
    {
        Slot& slot = m_meta_slot_array[slot_index];
        slot.set_prev_index(ordinal_index);
        slot_index = slot.get_next_index();
        ++ordinal_index;
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::append_range_to_loose_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept
{
    m_loose_count += static_cast<std::uint32_t>(upper_index - lower_index) + 1u;
    m_loose_list_head = combine_lists(m_loose_list_head, range_to_list(lower_index, upper_index, SlotState::is_loose_slot));
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::append_range_to_empty_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept
{
    m_empty_count += static_cast<std::uint32_t>(upper_index - lower_index) + 1u;
    m_empty_list_head = combine_lists(m_empty_list_head, range_to_list(lower_index, upper_index, SlotState::is_empty_slot));
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::attach_to_loose(const std::int32_t slot_index) noexcept
{
    Slot& slot = m_meta_slot_array[slot_index];
    slot.set_is_loose_slot();
    if (m_loose_list_head == -1)
    {
        slot.set_prev_index(slot_index);
        slot.set_next_index(slot_index);
    }
    else
    {
        slot.set_next_index(m_loose_list_head);
        slot.set_prev_index(m_meta_slot_array[m_loose_list_head].get_prev_index());
        m_meta_slot_array[slot.get_prev_index()].set_next_index(slot_index);
        m_meta_slot_array[slot.get_next_index()].set_prev_index(slot_index);
    }
    m_loose_list_head = slot_index;
    ++m_loose_count;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::attach_to_empty(const std::int32_t slot_index) noexcept
{
    Slot& slot = m_meta_slot_array[slot_index];
    slot.set_is_empty_slot();
    if (m_empty_list_head == -1)
    {
        slot.set_prev_index(slot_index);
        slot.set_next_index(slot_index);
    }
    else
    {
        slot.set_next_index(m_empty_list_head);
        slot.set_prev_index(m_meta_slot_array[m_empty_list_head].get_prev_index());
        m_meta_slot_array[slot.get_prev_index()].set_next_index(slot_index);
        m_meta_slot_array[slot.get_next_index()].set_prev_index(slot_index);
    }
    m_empty_list_head = slot_index;
    ++m_empty_count;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::remove_from_loose(const std::int32_t slot_index) noexcept
{
    Slot& slot = m_meta_slot_array[slot_index];
    --m_loose_count;
    if (m_loose_list_head == slot_index)
    {
        m_loose_list_head = (m_loose_count == 0) ? -1 : slot.get_next_index();
    }
    if (m_loose_count != 0)
    {
        m_meta_slot_array[slot.get_prev_index()].set_next_index(slot.get_next_index());
        m_meta_slot_array[slot.get_next_index()].set_prev_index(slot.get_prev_index());
    }
    slot.set_is_unassigned();
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::remove_from_empty(const std::int32_t slot_index) noexcept
{
    Slot& slot = m_meta_slot_array[slot_index];
    --m_empty_count;
    if (m_empty_list_head == slot_index)
    {
        m_empty_list_head = (m_empty_count == 0) ? -1 : slot.get_next_index();
    }
    if (m_empty_count != 0)
    {
        m_meta_slot_array[slot.get_prev_index()].set_next_index(slot.get_next_index());
        m_meta_slot_array[slot.get_next_index()].set_prev_index(slot.get_prev_index());
    }
    slot.set_is_unassigned();
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::move_to_loose_list(const std::int32_t slot_index) noexcept
{
    if (m_meta_slot_array[slot_index].is_empty_slot())
    {
        remove_from_empty(slot_index);
        attach_to_loose(slot_index);
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::move_to_empty_list(const std::int32_t slot_index) noexcept
{
    if (m_meta_slot_array[slot_index].is_loose_slot())
    {
        remove_from_loose(slot_index);
        attach_to_empty(slot_index);
    }
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::convert_to_rank_index(const std::int32_t slot_index) const noexcept
{
    std::int32_t rank_index = -1;
    if (m_meta_slot_array[slot_index].is_loose_slot())
    {
        rank_index = 0;
        for (std::int32_t scan_index = 0; scan_index != slot_index; ++scan_index)
        {
            if (m_meta_slot_array[scan_index].is_loose_slot())
            {
                ++rank_index;
            }
        }
    }
    return rank_index;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::locate_by_rank_index(const std::int32_t rank_index) const noexcept
{
    std::int32_t slot_index = -1;
    if ((rank_index >= 0) && (rank_index < m_loose_count))
    {
        for (std::uint32_t count = static_cast<std::uint32_t>(rank_index) + 1u; count != 0; --count)
        {
            for (++slot_index; !m_meta_slot_array[slot_index].is_loose_slot(); ++slot_index) {}
        }
    }
    return slot_index;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::min_occupied_index() const noexcept
{
    std::int32_t slot_index = 0;
    for (std::uint32_t slot_count = m_capacity; slot_count > 0; --slot_count)
    {
        Slot& slot = m_meta_slot_array[slot_index];
        if (!slot.is_empty_slot())
        {
            return static_cast<std::int32_t>(slot_index);
        }
        ++slot_index;
    }
    return -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::max_occupied_index() const noexcept
{
    std::int32_t slot_index = static_cast<std::int32_t>(m_capacity - 1);
    for (std::uint32_t slot_count = m_capacity; slot_count > 0; --slot_count)
    {
        Slot& slot = m_meta_slot_array[slot_index];
        if (!slot.is_empty_slot())
        {
            return static_cast<std::int32_t>(slot_index);
        }
        --slot_index;
    }
    return -1;
}

//  This function should only be called on construction or after a call to shutdown().
template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::move_from(TUnorderedSlots& src) noexcept
{
    m_capacity = src.m_capacity;
    m_peak_usage = src.m_peak_usage;
    m_peak_index = src.m_peak_index;
    m_high_index = src.m_high_index;
    m_loose_count = src.m_loose_count;
    m_empty_count = src.m_empty_count;
    m_loose_list_head = src.m_loose_list_head;
    m_empty_list_head = src.m_empty_list_head;
    m_meta_slot_array = src.m_meta_slot_array;
    m_lock = LockState::none;
    src.set_empty();
    return true;
}

//  This function should only be called on construction or after a call to shutdown().
template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::copy_from(const TUnorderedSlots& src) noexcept
{
    if (src.is_initialised())
    {
        m_meta_slot_array = memory::t_allocate<Slot>(static_cast<std::size_t>(src.m_capacity));
        if (m_meta_slot_array == nullptr)
        {
            return false;
        }

        m_capacity = src.m_capacity;
        m_peak_usage = src.m_peak_usage;
        m_peak_index = src.m_peak_index;
        m_high_index = src.m_high_index;
        m_loose_count = src.m_loose_count;
        m_empty_count = src.m_empty_count;
        m_loose_list_head = src.m_loose_list_head;
        m_empty_list_head = src.m_empty_list_head;
        m_lock = LockState::none;

        std::size_t bytes = (m_capacity * sizeof(Slot));
        std::memcpy(m_meta_slot_array, src.m_meta_slot_array, bytes);
    }
    else
    {
        set_empty();
    }
    return true;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::set_empty() noexcept
{
    m_capacity = 0;
    m_peak_usage = 0;
    m_peak_index = -1;
    m_high_index = -1;
    m_loose_count = 0;
    m_empty_count = 0;
    m_loose_list_head = -1;
    m_empty_list_head = -1;
    m_meta_slot_array = nullptr;
    m_lock = LockState::none;
}

using CUnorderedSlots_int16 = TUnorderedSlots<std::int16_t>;
using CUnorderedSlots_int32 = TUnorderedSlots<std::int32_t>;

#endif  //  TUNORDERED_SLOTS_HPP_INCLUDED
