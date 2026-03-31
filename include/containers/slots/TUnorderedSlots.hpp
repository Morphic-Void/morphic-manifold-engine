
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
//  In TUnorderedSlots, traversal order is list order.
//  Traversal order does not define rank.
//
//  Rank is defined over loose slots only.
//  rank(slot_index) == number of loose slots with lower slot index.
//  Empty slots have no rank and return -1.
//  Valid rank domain is [0, loose_count()).
//
//  pack() compacts loose payload into slot indices [0, loose_count())
//  and rebuilds the loose and empty lists in ascending slot index order.
//  pack() does not preserve empty-slot payload, does not define rank,
//  and does not provide full-domain remapping.
//
//  Do not assume that TOrderedSlots rank or remapping semantics apply here.
//
//
//  TUnorderedSlots<TIndex>
//
//  Overview
//  --------
//  TUnorderedSlots is a single-threaded unordered index over an external slot array.
//
//  The template owns and manages slot metadata only:
//      - Bi-directional list links (prev_index, next_index)
//      - Slot state (loose / empty / internal transitional states)
//      - Category counts and high/peak tracking
//
//  The derived class owns the payload and must implement:
//
//      on_visit(slot_index, identifier)
//      on_move_payload(source_index, target_index)
//      on_reserve_empty(minimum_capacity, recommended_capacity)
//
//
//  Scope
//  -----
//  - Metadata only; no payload ownership
//  - No payload construction or destruction
//  - Payload movement semantics are defined by the derived class
//
//
//  State model
//  -----------
//  Slot indices are integers in [0, capacity()).
//  Each slot index addresses both slot metadata and its corresponding derived payload element.
//
//  Steady-state slot categories:
//      Loose  - occupied, identity-bearing payload
//      Empty  - available free space
//
//  Steady-state invariants:
//      - m_loose_count + m_empty_count == m_capacity
//      - Loose and empty slots each form a circular bi-directional list
//      - m_high_index is the highest occupied slot index (or -1 if none)
//      - m_peak_usage and m_peak_index record historical maxima
//      - No slot belongs to more than one steady-state category
//
//  Internal transitional states are implementation detail only and are not part of the external model.
//
//
//  Observation model
//  -----------------
//  Traversal order:
//      - Defined strictly by loose/empty list order
//      - No rank information is supplied or implied during traversal
//
//  Rank:
//      - Defined strictly by ascending slot index over loose slots
//      - rank(slot_index) == number of loose slots with lower slot index
//      - Valid rank domain is [0, loose_count())
//      - Empty slots have no rank and return -1
//
//  Pack result:
//      - Loose payload is compacted into [0, loose_count())
//      - Remaining slots are Empty
//      - Loose and empty lists are rebuilt in ascending slot index order
//
//  Only loose slots are identity-bearing.
//  Empty slots are free space only.
//  No full-domain remapping guarantee is provided.
//
//
//  Lifecycle
//  ---------
//  - initialise(capacity) allocates metadata and marks all slots Empty
//  - shutdown() releases metadata and resets to uninitialised
//  - safe_resize()/reserve_empty() adjust capacity subject to invariants
//  - pack() compacts loose payload and rebuilds metadata
//
//
//  Locking and re-entry model
//  --------------------------
//  The template is strictly single-threaded.
//
//  During execution of a virtual callback the template enters a lock state:
//
//      LockState::on_visit
//      LockState::on_move_payload
//      LockState::on_reserve_empty
//
//  Bulk operations hold the corresponding lock for the duration of the coordinated phase.
//  Virtual callbacks execute within that locked phase.
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
//
//  Integrity and validation
//  ------------------------
//  - check_integrity() validates metadata invariants, counts, list structure, and index ranges
//
//
//  Capacity model
//  --------------
//  - Maximum supported slot index is std::numeric_limits<TIndex>::max()
//  - capacity_limit() == index_limit() + 1
//  - minimum_safe_capacity() == high_index() + 1
//  - Growth is constrained by the slot index domain, not just by loose_count()
//
//
//  Migration and alignment with TOrderedSlots
//  ------------------------------------------
//  TOrderedSlots and TUnorderedSlots define rank differently.
//
//  TOrderedSlots:
//      - Rank is full-domain
//      - Occupied and empty slots both participate in remapping
//      - pack()/sort_and_pack() perform total reordering
//
//  TUnorderedSlots:
//      - Rank is occupied-domain only
//      - Empty slots have no rank and no stable identity
//      - pack() performs occupied compaction only
//
//  Migration constraints:
//
//      - Do not assume empty-slot payload preservation
//      - Do not assume empty-slot remapping
//      - Do not assume full-domain rank behaviour
//
//      - Do not treat TOrderedSlots rank as occupied-only
//      - Do not treat ordered/full-domain operations as simple compaction
//
//  A derived class may implement on_move_payload() using swap-like behaviour.
//  In TUnorderedSlots this avoids destructive empty-slot behaviour.
//  In TOrderedSlots this is safe but typically less efficient than full-domain reordering.
// 
//  When migrating code written only for TUnorderedSlots, do not assume that
//  TOrderedSlots::on_move_payload() uses only non-negative indices.
//
//
//  Type constraints
//  ----------------
//  Supported types:
//      std::int32_t
//      std::int16_t
//
//  TIndex must be signed.
//  Slot metadata representation is constrained for compact flag/index encoding.
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

#include "SlotsRankMap.hpp"
#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"

namespace slots
{

/// Unordered metadata index over externally stored payload.
///
/// TUnorderedSlots stores slot metadata only. The derived class owns payload
/// storage and defines payload movement semantics.
///
/// See docs/TUnorderedSlots.md for the full model and terminology.
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

    //  Protected functions form the derived-facing interface.

private:

    //  Private functions are implementation details.

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

    //  Capacity management.
    //
    //  These functions preserve loose slots and do not compact payload.
    [[nodiscard]] bool safe_resize(const std::uint32_t requested_capacity) noexcept;
    [[nodiscard]] bool reserve_empty(const std::uint32_t slot_count) noexcept;
    [[nodiscard]] bool shrink_to_fit() noexcept;

    //  Acquire an empty slot and move it to the loose category.
    //
    //  slot_index == -1 selects the default empty slot.
    //  acquire() does not grow capacity.
    //  reserve_and_acquire() may grow capacity first.
    //  Both return the acquired slot index, or -1 on failure.
    [[nodiscard]] std::int32_t acquire(const std::int32_t slot_index = -1) noexcept;
    [[nodiscard]] std::int32_t reserve_and_acquire(const std::int32_t slot_index = -1) noexcept;

    //  Return a loose slot to the empty category.
    //
    //  Payload handling is the responsibility of the derived class.
    //  Returns false if slot_index is invalid or not loose.
    bool erase(const std::int32_t slot_index) noexcept;

    //  Slot-category queries by slot index.
    //
    //  is_safe_slot() checks that the structure is safe and
    //  slot_index is in [0, capacity()).
    //
    //  is_loose_slot() and is_empty_slot() add the corresponding category check.
    [[nodiscard]] bool is_safe_slot(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool is_loose_slot(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool is_empty_slot(const std::int32_t slot_index) const noexcept;

    //  Loose-list traversal by slot index.
    //
    //  These functions operate in loose-list order only.
    //  They do not imply or expose occupied-domain rank order.
    //
    //  first_loose()/last_loose() return the list head/tail, or -1 if there
    //  are no loose slots or the structure is not safe.
    //
    //  prev_loose()/next_loose() return the adjacent loose slot in list order,
    //  or -1 if slot_index is invalid or the slot not loose or at the end of the list.
    [[nodiscard]] std::int32_t first_loose() const noexcept;
    [[nodiscard]] std::int32_t last_loose() const noexcept;
    [[nodiscard]] std::int32_t prev_loose(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t next_loose(const std::int32_t slot_index) const noexcept;

    //  Build an occupied-domain rank/slot mapping for loose slots.
    //
    //  Rank is defined by ascending slot index over loose slots only.
    //  Empty slots do not participate and map to -1.
    //
    //  RankMap size is capacity().
    //  rank_to_slot is valid for ranks in [0, loose_count()).
    //  slot_to_rank is valid for loose slots only.
    //  All other entries use -1 sentinels.
    //
    //  On failure, or if no mapping can be built, the returned RankMap is empty.
    [[nodiscard]] RankMap build_rank_map() const noexcept;

    //  Compact loose payload into the lowest slot indices.
    //
    //  After completion:
    //      - Loose payload occupies slot indices [0, loose_count()).
    //      - Remaining slots are Empty.
    //      - Loose and empty lists are rebuilt in ascending slot index order.
    //
    //  Uses on_move_payload() to coordinate derived payload movement.
    //  Empty-slot payload is not preserved.
    //  This operation does not define rank and does not provide full-domain remapping.
    void pack() noexcept;

    //  Rebuild the loose list in ascending slot index order.
    //
    //  After completion, loose list order and rank order are identical.
    void rebuild_loose_in_index_order() noexcept;

    //  Rebuild the empty list in ascending slot index order.
    //
    //  Empty slots do not participate in rank.
    void rebuild_empty_in_index_order() noexcept;

    //  Return the rank of a loose slot by slot index.
    //
    //  Rank is the number of loose slots with lower slot index.
    //  Returns -1 if the slot is invalid or Empty.
    [[nodiscard]] std::int32_t rank_index_of(const std::int32_t slot_index) const noexcept;

    //  Return the slot index of a loose slot by rank.
    //
    //  Valid rank domain is [0, loose_count()).
    //  Returns -1 if rank_index is out of range.
    [[nodiscard]] std::int32_t find_by_rank_index(const std::int32_t rank_index) const noexcept;

    //  Visit one or more slot categories in list order.
    //
    //  For each visited slot, calls on_visit(slot_index, identifier).
    //  identifier is category-derived and is not a slot index or rank:
    //
    //      - identifier == -1 for loose slots
    //      - identifier == -2 for empty slots
    //
    //  visit_all() visits loose first, then empty.
    void visit_loose() noexcept;
    void visit_empty() noexcept;
    void visit_all() noexcept;

    //  Validate metadata integrity in stable state.
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

    /// Visit callback for category traversal in list order.
    ///
    /// identifier is category-derived and is not a slot index or rank:
    ///
    ///     - identifier == -1 for loose slots
    ///     - identifier == -2 for empty slots
    virtual void on_visit(const std::int32_t slot_index, const std::int32_t identifier) noexcept
    {
        (void)slot_index;
    }

    /// Move derived payload during coordinated compaction.
    ///
    /// Called only by pack().
    ///
    /// TUnorderedSlots contract:
    ///   - source_index != target_index
    ///   - source_index and target_index are non-negative
    ///   - source_index references a loose slot
    ///   - target_index is in [0, loose_count())
    ///   - target_index may currently hold empty-slot payload
    ///
    /// Empty-slot payload preservation is not required.
    /// The derived class must implement overwrite-safe behaviour.
    /// A preserving implementation may use swap-like movement, but the template
    /// guarantees compaction only.
    ///
    /// Migration note:
    ///   - TOrderedSlots may use -1 to denote derived temporary storage.
    ///   - TUnorderedSlots does not use -1 here.
    ///   - Code written for TOrderedSlots is usually safe here.
    ///   - Code written only for TUnorderedSlots may be unsafe in TOrderedSlots.
    virtual void on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
    {
        (void)source_index;
        (void)target_index;
    }

    /// Approve and finalise growth for reserve_empty() or reserve_and_acquire().
    ///
    /// Returns the absolute capacity to apply.
    /// Returning a value less than minimum_capacity causes the caller to fail.
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

    //  Private guarded virtual-call helpers.
    //  safe_on_visit() computes identifier from the visited slot category.
    //  safe_on_visit() and safe_on_move_payload() are available single-call wrappers.
    //  Batched dispatch is usually preferable.
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
    {   //  Slot metadata structure
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

    //  Integrity check functions.
    static inline bool failed_integrity_check() noexcept;
    [[nodiscard]] bool private_integrity_check() const noexcept;

    //  Dispatch batched on_visit() calls for the selected categories.
    //  Visits loose first, then empty, preserving list order within each category.
    void private_on_visit_dispatcher(const bool visit_loose, const bool visit_empty) noexcept;

    //  Resize implementation after precondition validation.
    [[nodiscard]] bool private_resize(const std::uint32_t requested_capacity) noexcept;

    //  Acquire implementation with optional reservation.
    [[nodiscard]] std::int32_t private_acquire(const std::int32_t slot_index, const bool allow_reserve) noexcept;

    //  Implementation of pack().
    //
    //  Compacts loose payload into the lowest slot indices and then rebuilds
    //  loose and empty metadata in ascending slot index order.
    //
    //  on_move_payload(source_index, target_index) is called only for loose
    //  slots outside the compacted prefix.
    //
    //  Empty-slot payload is not preserved unless the derived class preserves
    //  it inside on_move_payload().
    void private_compact() noexcept;

    //  Scan an inclusive slot-index range and build a bi-directional list of slots
    //  in the specified category.
    //  The caller is responsible for correcting any orphaned list members.
    //  Returns the list head, or -1 if no matching slots are found.
    [[nodiscard]] std::int32_t state_to_list(const std::int32_t lower_index, const std::int32_t upper_index, const SlotState state) noexcept;

    //  Convert an inclusive slot-index range to a bi-directional list.
    //  The caller must ensure these slots are not currently managed.
    //  Returns the list head, or -1 if the range is empty or invalid.
    [[nodiscard]] std::int32_t range_to_list(const std::int32_t lower_index, const std::int32_t upper_index, const SlotState state) noexcept;

    //  Concatenate two bi-directional lists by appending list2 to list1.
    //  Returns the head of the combined list.
    [[nodiscard]] std::int32_t combine_lists(const std::int32_t list1_head_index, const std::int32_t list2_head_index) noexcept;

    //  Append a slot-index range to the loose or empty list.
    //  The caller must ensure these slots are not currently managed.
    void append_range_to_loose_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept;
    void append_range_to_empty_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept;

    //  Internal helper functions for the move_to_* functions.
    void attach_to_loose(const std::int32_t slot_index) noexcept;
    void attach_to_empty(const std::int32_t slot_index) noexcept;
    void remove_from_loose(const std::int32_t slot_index) noexcept;
    void remove_from_empty(const std::int32_t slot_index) noexcept;

    //  Move a slot to the specified metadata category if not already a member.
    void move_to_loose_list(const std::int32_t slot_index) noexcept;
    void move_to_empty_list(const std::int32_t slot_index) noexcept;

    //  Convert a slot index to occupied-domain rank.
    //
    //  Rank is defined by ascending slot index over loose slots only.
    //  Returns -1 if the slot is not loose.
    [[nodiscard]] std::int32_t convert_to_rank_index(const std::int32_t slot_index) const noexcept;

    //  Locate a loose slot by occupied-domain rank.
    //
    //  Valid rank domain is [0, loose_count()).
    //  Returns the corresponding slot index, or -1 if rank_index is out of range.
    [[nodiscard]] std::int32_t locate_by_rank_index(const std::int32_t rank_index) const noexcept;

    //  Scan for the lowest/highest non-empty slot index in the metadata array.
    [[nodiscard]] std::int32_t min_occupied_index() const noexcept;
    [[nodiscard]] std::int32_t max_occupied_index() const noexcept;

    //  These functions should only be called on construction or after a call to shutdown().
    bool move_from(TUnorderedSlots& src) noexcept;
    bool copy_from(const TUnorderedSlots& src) noexcept;
    void set_empty() noexcept;

private:

    //  Private data.
    std::uint32_t m_capacity = 0u;          //  allocated slot count
    std::uint32_t m_peak_usage = 0u;        //  peak occupied slot count
    std::int32_t  m_peak_index = -1;        //  peak occupied slot index
    std::int32_t  m_high_index = -1;        //  highest currently occupied index
    std::uint32_t m_loose_count = 0u;       //  count of slots in the loose list
    std::uint32_t m_empty_count = 0u;       //  count of slots in the empty list
    std::int32_t  m_loose_list_head = -1;   //  index of the loose slot list head (or -1)
    std::int32_t  m_empty_list_head = -1;   //  index of the empty slot list head (or -1)

    memory::TMemoryToken<Slot>  m_meta_slot_array;  //  slot meta data array

    //  Private lock state.
    mutable LockState m_lock = LockState::none;

    //  Constants
    static constexpr std::uint32_t k_capacity_limit =
        static_cast<std::uint32_t>(std::min(
            memory::t_max_elements<Slot>(),
            static_cast<std::size_t>(std::numeric_limits<TIndex>::max() + 1u)));

    static constexpr std::int32_t k_index_limit = static_cast<std::int32_t>(k_capacity_limit - 1u);

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
        "TUnorderedSlots: Supported types are std::int32_t and std::int16_t.");

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
        m_meta_slot_array.deallocate();
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
                const Slot* const meta = m_meta_slot_array.data();
                for (--m_high_index; !meta[m_high_index].is_loose_slot(); --m_high_index) {}
            }
        }
        return true;
    }
    return false;
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::is_safe_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe() && (static_cast<std::uint32_t>(slot_index) < m_capacity);
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::is_loose_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) && m_meta_slot_array.data()[slot_index].is_loose_slot();
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::is_empty_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) && m_meta_slot_array.data()[slot_index].is_empty_slot();
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::first_loose() const noexcept
{
    return is_safe() ? m_loose_list_head : -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::last_loose() const noexcept
{
    return (is_safe() && (m_loose_list_head != -1)) ? m_meta_slot_array.data()[m_loose_list_head].get_prev_index() : -1;
}


template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::prev_loose(const std::int32_t slot_index) const noexcept
{
    if (is_loose_slot(slot_index) && (slot_index != m_loose_list_head))
    {
        return m_meta_slot_array.data()[slot_index].get_prev_index();
    }
    return -1;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::next_loose(const std::int32_t slot_index) const noexcept
{
    if (is_loose_slot(slot_index))
    {
        const std::int32_t next_index = m_meta_slot_array.data()[slot_index].get_next_index();
        if (next_index != m_loose_list_head)
        {
            return next_index;
        }
    }
    return -1;
}

template<typename TIndex>
[[nodiscard]] RankMap TUnorderedSlots<TIndex>::build_rank_map() const noexcept
{
    RankMap rank_map;
    if (is_safe() && (m_capacity != 0u))
    {
        if (rank_map.allocate(static_cast<std::size_t>(m_capacity)))
        {
            (void)rank_map.set_size(static_cast<std::size_t>(m_capacity));
            RankMapEntry* const map = rank_map.data();
            std::fill_n(map, rank_map.size(), RankMapEntry{});
            const Slot* const meta = m_meta_slot_array.data();
            std::int32_t rank_index = 0;
            std::int32_t slot_index = 0;
            for (std::uint32_t count = static_cast<std::uint32_t>(m_high_index) + 1u; count != 0; --count)
            {
                if (meta[slot_index].is_loose_slot())
                {
                    map[rank_index].rank_to_slot = slot_index;
                    map[slot_index].slot_to_rank = rank_index;
                    ++rank_index;
                }
                ++slot_index;
            }
            MV_HARD_ASSERT(rank_index == static_cast<std::int32_t>(m_loose_count));
        }
    }
    return rank_map;
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
inline std::int32_t TUnorderedSlots<TIndex>::rank_index_of(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) ? convert_to_rank_index(slot_index) : -1;
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
    return MV_FAIL_SAFE_ASSERT(m_lock == LockState::none) && (allow_null || (m_meta_slot_array.data() != nullptr));
}

template<typename TIndex>
inline bool TUnorderedSlots<TIndex>::lock(const LockState lock, const bool allow_null) const noexcept
{
    bool success = false;
    if (MV_FAIL_SAFE_ASSERT(m_lock == LockState::none) && (allow_null || (m_meta_slot_array.data() != nullptr)))
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
        switch (m_meta_slot_array.data()[slot_index].get_slot_state())
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
    const Slot* const meta = m_meta_slot_array.data();
    if (meta != nullptr)
    {
        if ((std::uintptr_t(meta) % alignof(Slot)) != 0u)
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
            const Slot& slot = meta[slot_index];
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
                const Slot& slot = meta[empty_index];
                if (!slot.is_empty_slot())
                {   //  the empty list links to a non-empty slot
                    return failed_integrity_check();
                }
                if (meta[slot.get_next_index()].get_prev_index() != empty_index)
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
                const Slot& slot = meta[loose_index];
                if (!slot.is_loose_slot())
                {   //  the loose list links to a non-loose slot
                    return failed_integrity_check();
                }
                if (meta[slot.get_next_index()].get_prev_index() != loose_index)
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
    const Slot* const meta = m_meta_slot_array.data();
    if (meta != nullptr)
    {
        if (visit_loose)
        {
            std::int32_t slot_index = m_loose_list_head;
            for (std::uint32_t loose_count = m_loose_count; loose_count != 0; --loose_count)
            {
                on_visit(slot_index, -1);
                slot_index = meta[slot_index].get_next_index();
            }
        }
        if (visit_empty)
        {
            std::int32_t slot_index = m_empty_list_head;
            for (std::uint32_t empty_count = m_empty_count; empty_count != 0; --empty_count)
            {
                on_visit(slot_index, -2);
                slot_index = meta[slot_index].get_next_index();
            }
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
        else if (m_meta_slot_array.data() == nullptr)
        {   //  initialisation
            if (m_meta_slot_array.allocate(static_cast<std::size_t>(requested_capacity), false))
            {
                m_capacity = m_empty_count = requested_capacity;
                m_empty_list_head = range_to_list(0, static_cast<std::int32_t>(m_empty_count - 1), SlotState::is_empty_slot);
                resized = true;
            }
        }
        else if (requested_capacity >= minimum_safe_capacity())
        {
            Slot* new_meta_slot_array = memory::t_allocate<Slot>(static_cast<std::size_t>(requested_capacity));
            if (m_meta_slot_array.reallocate(static_cast<std::size_t>(std::min(requested_capacity, m_capacity)), static_cast<std::size_t>(requested_capacity), false))
            {
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
        else if ((static_cast<std::uint32_t>(slot_index) < m_capacity) && m_meta_slot_array.data()[slot_index].is_empty_slot())
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
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t target_index = 0;
        std::uint32_t count = m_loose_count;
        for (std::int32_t source_index = 0; count != 0; ++source_index)
        {
            if (meta[source_index].is_loose_slot())
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
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t prev_index = -1;
        std::int32_t next_index = -1;
        std::uint32_t scan_count = 0;
        for (std::int32_t scan_index = upper_index; scan_index >= lower_index; --scan_index)
        {   //  scan backwards creating a singly linked forward list
            Slot& slot = meta[scan_index];
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
                Slot& slot = meta[next_index];
                slot.set_prev_index(prev_index);
                prev_index = next_index;
                next_index = slot.get_next_index();
                --scan_count;
            }

            //  fix up the list to be circular (next_index is silently invalid at this point)
            meta[head_index].set_prev_index(prev_index);
            meta[prev_index].set_next_index(head_index);
        }
    }
    return head_index;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::range_to_list(const std::int32_t lower_index, const std::int32_t upper_index, SlotState state) noexcept
{
    if ((lower_index <= upper_index) && (lower_index >= 0))
    {
        Slot* const meta = m_meta_slot_array.data();
        for (std::int32_t scan_index = lower_index; scan_index <= upper_index; ++scan_index)
        {   //  scan the range creating new list members
            Slot& slot = meta[scan_index];
            slot.set_prev_index(scan_index - 1);    //  potentially transiently invalid
            slot.set_next_index(scan_index + 1);    //  potentially transiently invalid
            slot.set_slot_state(state);
        }

        //  fix up the list to be circular (and correct any transiently invalid index values)
        meta[upper_index].set_next_index(lower_index);
        meta[lower_index].set_prev_index(upper_index);
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
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t list1_tail_index = meta[list1_head_index].get_prev_index();
        std::int32_t list2_tail_index = meta[list2_head_index].get_prev_index();
        meta[list1_head_index].set_prev_index(list2_tail_index);
        meta[list1_tail_index].set_next_index(list2_head_index);
        meta[list2_head_index].set_prev_index(list1_tail_index);
        meta[list2_tail_index].set_next_index(list1_head_index);
    }
    return list1_head_index;
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
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    slot.set_is_loose_slot();
    if (m_loose_list_head == -1)
    {
        slot.set_prev_index(slot_index);
        slot.set_next_index(slot_index);
    }
    else
    {
        slot.set_next_index(m_loose_list_head);
        slot.set_prev_index(meta[m_loose_list_head].get_prev_index());
        meta[slot.get_prev_index()].set_next_index(slot_index);
        meta[slot.get_next_index()].set_prev_index(slot_index);
    }
    m_loose_list_head = slot_index;
    ++m_loose_count;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::attach_to_empty(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    slot.set_is_empty_slot();
    if (m_empty_list_head == -1)
    {
        slot.set_prev_index(slot_index);
        slot.set_next_index(slot_index);
    }
    else
    {
        slot.set_next_index(m_empty_list_head);
        slot.set_prev_index(meta[m_empty_list_head].get_prev_index());
        meta[slot.get_prev_index()].set_next_index(slot_index);
        meta[slot.get_next_index()].set_prev_index(slot_index);
    }
    m_empty_list_head = slot_index;
    ++m_empty_count;
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::remove_from_loose(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    --m_loose_count;
    if (m_loose_list_head == slot_index)
    {
        m_loose_list_head = (m_loose_count == 0) ? -1 : slot.get_next_index();
    }
    if (m_loose_count != 0)
    {
        meta[slot.get_prev_index()].set_next_index(slot.get_next_index());
        meta[slot.get_next_index()].set_prev_index(slot.get_prev_index());
    }
    slot.set_is_unassigned();
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::remove_from_empty(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    --m_empty_count;
    if (m_empty_list_head == slot_index)
    {
        m_empty_list_head = (m_empty_count == 0) ? -1 : slot.get_next_index();
    }
    if (m_empty_count != 0)
    {
        meta[slot.get_prev_index()].set_next_index(slot.get_next_index());
        meta[slot.get_next_index()].set_prev_index(slot.get_prev_index());
    }
    slot.set_is_unassigned();
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::move_to_loose_list(const std::int32_t slot_index) noexcept
{
    if (m_meta_slot_array.data()[slot_index].is_empty_slot())
    {
        remove_from_empty(slot_index);
        attach_to_loose(slot_index);
    }
}

template<typename TIndex>
inline void TUnorderedSlots<TIndex>::move_to_empty_list(const std::int32_t slot_index) noexcept
{
    if (m_meta_slot_array.data()[slot_index].is_loose_slot())
    {
        remove_from_loose(slot_index);
        attach_to_empty(slot_index);
    }
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::convert_to_rank_index(const std::int32_t slot_index) const noexcept
{
    std::int32_t rank_index = -1;
    const Slot* const meta = m_meta_slot_array.data();
    if (meta[slot_index].is_loose_slot())
    {
        rank_index = 0;
        for (std::int32_t scan_index = 0; scan_index != slot_index; ++scan_index)
        {
            if (meta[scan_index].is_loose_slot())
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
        const Slot* const meta = m_meta_slot_array.data();
        for (std::uint32_t count = static_cast<std::uint32_t>(rank_index) + 1u; count != 0; --count)
        {
            for (++slot_index; !meta[slot_index].is_loose_slot(); ++slot_index) {}
        }
    }
    return slot_index;
}

template<typename TIndex>
inline std::int32_t TUnorderedSlots<TIndex>::min_occupied_index() const noexcept
{
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t slot_index = 0;
    for (std::uint32_t slot_count = m_capacity; slot_count > 0; --slot_count)
    {
        Slot& slot = meta[slot_index];
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
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t slot_index = static_cast<std::int32_t>(m_capacity - 1);
    for (std::uint32_t slot_count = m_capacity; slot_count > 0; --slot_count)
    {
        Slot& slot = meta[slot_index];
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
    m_meta_slot_array = std::move(src.m_meta_slot_array);
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
        if (!m_meta_slot_array.clone(src.m_meta_slot_array, src.m_capacity))
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
    m_meta_slot_array.deallocate();
    m_lock = LockState::none;
}

using CUnorderedSlots_int16 = TUnorderedSlots<std::int16_t>;
using CUnorderedSlots_int32 = TUnorderedSlots<std::int32_t>;

}   //  namespace slots

#endif  //  TUNORDERED_SLOTS_HPP_INCLUDED
