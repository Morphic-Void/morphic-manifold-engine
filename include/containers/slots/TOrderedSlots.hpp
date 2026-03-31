
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   TOrderedSlots.hpp
//  Author: Ritchie Brannan
//  Date:   10 Jan 26
//
//  TOrderedSlots<TIndex, TMeta>
//
//  Metadata-only ordering and slot-management toolkit over an
//  externally owned slot-aligned payload domain.
//
//  IMPORTANT TERMINOLOGY NOTE
//  --------------------------
//  In this component, "lexed" means ordered by on_compare_keys().
//
//  It does NOT imply:
//      - string comparison
//      - text collation
//      - lexicographic ordering in the narrow textual sense
//
//  The ordering relation is entirely defined by the derived class.
//
//  TOOLKIT ROLE
//  ------------
//  TOrderedSlots is a structural toolkit, not a complete container.
//
//  It owns and manages slot metadata only:
//      - AVL tree links (parent_index, child_index[2])
//      - Balance factors
//      - Slot state (lexed / loose / empty / unassigned)
//      - Category counts and high/peak tracking
//
//  It does not own:
//      - payload items
//      - query keys
//      - payload movement policy
//      - external growth policy
//
//  A concrete derived class supplies whichever callback behaviour its
//  intended use actually requires, and may expose only the subset of
//  inherited functionality that matches its semantics.
//
//  Typical derived responsibilities may include:
//      - key comparison for ordered operations
//      - payload movement for sort_and_pack()
//      - visit handling for traversal callbacks
//      - reserve/growth approval policy
//
//  A derived class is not required to expose or use every capability
//  provided by this toolkit.
//
//  ORDERING MODEL
//  --------------
//  Occupied slots may exist in one of two occupied categories:
//
//      Lexed  - occupied and a member of the ordered AVL subset
//      Loose  - occupied but not in the ordered subset
//
//  Empty slots are available for acquisition.
//
//  Ordered traversal and ordered rank are defined by in-order traversal
//  of the lexed slots, followed by the loose slots and then empty slots.
//
//  In TOrderedSlots, TRAVERSAL ORDER DEFINES RANK ORDER.
//
//  After sort_and_pack(), traversal order, rank order, and slot index
//  order are identical within the occupied region.
//
//  Do not assume that this behaviour applies to TUnorderedSlots.
//
//  ORDER DEFINITION
//  ----------------
//  Lexed ordering is defined solely by on_compare_keys().
//
//  For equal comparisons (on_compare_keys(a, b) == 0), ordering is
//  stable by insertion order. Inserting an equal key appends it as the
//  in-order successor of the last equal key. relex_all() and
//  sort_and_pack() preserve the existing in-order sequence,
//  including equal-key runs.
//
//  QUERY KEY MODEL
//  ---------------
//  Some search and acquisition operations call:
//
//      on_compare_keys(-1, target_index)
//
//  The template does not store the query key. The derived class must
//  stage the current query or insert key in its own storage before
//  calling these operations, and keep it valid for the duration of the
//  call.
//
//  SLOT MODEL
//  ----------
//  Slot indices are integers in [0, capacity()). Each index addresses
//  both slot metadata and its corresponding derived payload element or
//  payload-side record, if any.
//
//  STEADY-STATE INVARIANTS
//  -----------------------
//  - m_lexed_count + m_loose_count + m_empty_count == m_capacity
//  - Lexed slots form a valid AVL tree
//  - Loose and empty slots form circular bi-directional lists
//  - m_high_index is the highest occupied slot index (or -1 if none)
//  - m_peak_usage and m_peak_index record historical maxima
//  - No slot belongs to more than one category
//
//  LIFECYCLE
//  ---------
//  - initialise(capacity) allocates metadata and marks all slots Empty
//  - shutdown() releases metadata and resets to uninitialised
//  - safe_resize()/reserve_empty() adjust capacity subject to invariants
//  - sort_and_pack() optionally reorders payload-side state and rebuilds
//    metadata, if that operation is meaningful for the derived class
//
//  After sort_and_pack():
//      - Lexed items occupy slot indices [0, lexed_count()) in order
//      - Loose items occupy slot indices [lexed_count(), lexed_count() + loose_count())
//      - Remaining slots are Empty
//      - The lexed metadata is rebuilt as a balanced AVL tree
//
//  CALLBACK OPTIONALITY
//  --------------------
//  on_compare_keys():
//      Required only for ordered operations that depend on key order,
//      such as lexed acquisition, find/bound queries, duplicate-key
//      checks, lex/relex operations, and lexical validation.
//
//  on_visit():
//      Required only if visit_*() traversal callbacks are used.
//
//  on_move_payload():
//      Required only if sort_and_pack() is used meaningfully by the
//      derived class.
//
//  on_reserve_empty():
//      Optional growth approval / adjustment hook.
//      The default implementation accepts the recommended capacity.
//
//  LOCKING AND RE-ENTRY MODEL
//  --------------------------
//  The template is strictly single-threaded.
//
//  During execution of a virtual callback the template enters a lock
//  state:
//
//      LockState::on_visit
//      LockState::on_move_payload
//      LockState::on_reserve_empty
//      LockState::on_compare_keys
//
//  While locked:
//      - Only explicitly safe accessor functions may be called
//      - All other protected functions are unsafe
//      - Debug: unsafe calls hard-fail
//      - Release: unsafe calls soft-fail (return false / -1, no mutation)
//
//  Integrity checks are valid only in stable state, not during mutation
//  or callback dispatch.
//
//  Functions safe during virtual callbacks:
//
//      is_initialised(), capacity(), capacity_limit(), minimum_safe_capacity()
//      peak_usage(), peak_index(), high_index(), index_limit()
//      lexed_count(), loose_count(), empty_count(), occupied_count()
//
//  These functions are non-mutating, do not acquire locks, do not invoke
//  virtual functions, and do not call is_safe().
//
//  INTEGRITY AND VALIDATION
//  ------------------------
//  - validate_tree(check_lex_order) verifies AVL structure and balance;
//    if check_lex_order is true, comparator-defined ordering is validated
//    via on_compare_keys()
//  - check_integrity() validates metadata invariants, counts, list
//    structure, tree balance, and index ranges; it does not validate
//    comparator ordering
//
//  CAPACITY CONSTRAINTS
//  --------------------
//  - Maximum supported slot index is std::numeric_limits<TIndex>::max()
//  - capacity_limit() == index_limit() + 1
//  - minimum_safe_capacity() == (m_high_index + 1)
//
//  TYPE CONSTRAINTS
//  ----------------
//  Supported type pairs:
//      (std::int32_t, std::int16_t)
//      (std::int16_t, std::int8_t)
//
//  Slot metadata layout is constrained for predictable packing.
//  Slot must be trivially copyable.

#pragma once

#ifndef TORDERED_SLOTS_HPP_INCLUDED
#define TORDERED_SLOTS_HPP_INCLUDED

#include <algorithm>    //  std::fill_n, std::max, std::min
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::int8_t, std::int16_t, std::int32_t, std::uint32_t, std::uintptr_t
#include <cstring>      //  std::memcpy
#include <limits>       //  std::numeric_limits
#include <type_traits>  //  std::is_trivially_copyable_v, std::is_signed_v, std::is_same_v

#include "SlotsRankMap.hpp"
#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "debug/debug.hpp"

namespace slots
{

/// An ordered index over slot metadata for externally stored payload items.
///
/// TOrderedSlots stores only slot metadata (tree/list links, balance, occupancy).
/// The derived class owns the payload items, defines how keys are compared and
/// how payload items are moved between slots.
///
/// See docs/TOrderedSlots.md for full terminology and usage patterns.
template<typename TIndex = std::int32_t, typename TMeta = std::int16_t>
class TOrderedSlots
{
public:
    TOrderedSlots() noexcept = default;
    TOrderedSlots(TOrderedSlots&& src) noexcept { set_empty(); (void)move_from(src); }
    TOrderedSlots(const TOrderedSlots& src) noexcept { set_empty(); (void)copy_from(src); }
    TOrderedSlots(const std::uint32_t capacity) noexcept { (void)initialise(capacity); }
    virtual ~TOrderedSlots() noexcept { (void)shutdown(); }

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

    [[nodiscard]] std::uint32_t lexed_count() const noexcept;
    [[nodiscard]] std::uint32_t loose_count() const noexcept;
    [[nodiscard]] std::uint32_t empty_count() const noexcept;
    [[nodiscard]] std::uint32_t occupied_count() const noexcept;

    [[nodiscard]] static constexpr std::uint32_t index_limit() { return k_index_limit; }
    [[nodiscard]] static constexpr std::uint32_t capacity_limit() { return k_capacity_limit; }

protected:

    //  These functions are unsafe to call during virtual calls from this template.

    TOrderedSlots& operator=(TOrderedSlots&& src) noexcept;
    TOrderedSlots& operator=(const TOrderedSlots& src) noexcept;

    [[nodiscard]] bool take(TOrderedSlots& src) noexcept;
    [[nodiscard]] bool clone(const TOrderedSlots& src) noexcept;

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

    //  Acquire an empty slot as either loose or lexed.
    //  If slot_index is -1, acquires the first slot from the empty list.
    //  Returns the acquired slot index, or -1 on failure.
    [[nodiscard]] std::int32_t acquire(const std::int32_t slot_index = -1, const bool lex = false, const bool require_unique = false) noexcept;

    //  Reserve space to acquire the requested slot and then acquire it.
    //  Returns the acquired slot index, or -1 on failure.
    [[nodiscard]] std::int32_t reserve_and_acquire(const std::int32_t slot_index = -1, const bool lex = false, const bool require_unique = false) noexcept;

    //  Assign a slot to the empty list removing it from the lexed tree or loose list as appropriate.
    //  The derived class is responsible for handling or discarding the payload item(s).
    //  Returns false if the slot was invalid or empty.
    bool erase(const std::int32_t slot_index) noexcept;

    //  Slot categorisation functions.
    [[nodiscard]] bool is_occupied(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool is_safe_slot(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool is_lexed_slot(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool is_loose_slot(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool is_empty_slot(const std::int32_t slot_index) const noexcept;

    //  Traversal (in lex order) of lexed payload items using slot indices.
    [[nodiscard]] std::int32_t first_lexed() const noexcept;
    [[nodiscard]] std::int32_t last_lexed() const noexcept;
    [[nodiscard]] std::int32_t prev_lexed(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t next_lexed(const std::int32_t slot_index) const noexcept;

    //  Traversal of loose payload items using slot indices.
    [[nodiscard]] std::int32_t first_loose() const noexcept;
    [[nodiscard]] std::int32_t last_loose() const noexcept;
    [[nodiscard]] std::int32_t prev_loose(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t next_loose(const std::int32_t slot_index) const noexcept;

    //  Duplicate key checking.
    [[nodiscard]] bool has_duplicate_key(const std::int32_t slot_index = -1) const noexcept;
    [[nodiscard]] bool has_duplicate_key_in_lexed(const std::int32_t slot_index = -1) const noexcept;
    [[nodiscard]] bool has_duplicate_key_in_loose(const std::int32_t slot_index = -1) const noexcept;

    //  Transfer a slot from the loose list to the lexed tree.
    //  Returns false if uninitialised or the slot is not in the loose list.
    bool lex(const std::int32_t slot_index) noexcept;

    //  Transfer a slot from the lexed tree to the loose list.
    //  Returns false if uninitialised or the slot is not lexed.
    bool unlex(const std::int32_t slot_index) noexcept;

    //  Remove and re-insert a slot from/to the lexed tree.
    //  Useful if a slot key has, or may have, changed.
    //  Returns false if uninitialised or the slot is not lexed.
    bool relex(const std::int32_t slot_index) noexcept;

    //  Transfer all slots from the loose list to the lexed tree.
    void lex_all() noexcept;

    //  Transfer all slots from the lexed tree to the loose list.
    void unlex_all() noexcept;

    //  Remove and re-insert all slots from/to the lexed tree.
    //  Useful if multiple slot keys have, or may have changed.
    //  Can result in a marginally more balanced tree.
    void relex_all() noexcept;

    //  Build bidirectional rank/slot mapping for current slot state.
    //  Traversal order is lexed, then loose, then empty.
    [[nodiscard]] RankMap build_rank_map() const noexcept;

    //  Physically reorder payload items into canonical packed order.
    //
    //  After completion:
    //      - Lexed slots occupy [0, lexed_count) in lex order.
    //      - Loose slots occupy [lexed_count, lexed_count + loose_count).
    //      - Empty slots occupy [lexed_count + loose_count, capacity()).
    //      - Lexed metadata is rebuilt as a balanced AVL tree.
    //      - Loose and empty metadata are rebuilt as linear lists.
    //
    //  Payload movement is performed via on_move_payload():
    //      - In-place mode: uses cycle resolution (temporary slot index -1).
    //      - External mode: performs a single pass to a complete external domain.
    //
    //  See docs/TOrderedSlots.md for further detail.
    void sort_and_pack(const bool use_external_payload = false) noexcept;

    //  Index order list rebuilding utilities.
    //  For loose slots index order == rank_index order.
    void rebuild_loose_in_index_order() noexcept;
    void rebuild_empty_in_index_order() noexcept;

    //  Return the ranked index of a payload item by slot index, or -1 if the slot is empty.
    [[nodiscard]] std::int32_t rank_index_of(const std::int32_t slot_index) const noexcept;

    //  Return the slot index of a payload item by its ranked index, or -1 if the ranked index out of range.
    [[nodiscard]] std::int32_t find_by_rank_index(const std::int32_t rank_index) const noexcept;

    //  Find_* functions only search lexed slots.
    //  Uses on_compare_keys(-1, slot_index), where -1 indicates the derived class's currently staged query key.
    //  Returns the slot_index of a matching lexed slot, or -1 if there are no lexed matches.
    [[nodiscard]] std::int32_t find_any_equal() const noexcept;
    [[nodiscard]] std::int32_t find_first_equal() const noexcept;
    [[nodiscard]] std::int32_t find_first_greater() const noexcept;
    [[nodiscard]] std::int32_t find_first_greater_equal() const noexcept;
    [[nodiscard]] std::int32_t find_last_equal() const noexcept;
    [[nodiscard]] std::int32_t find_last_less() const noexcept;
    [[nodiscard]] std::int32_t find_last_less_equal() const noexcept;

    //  Aliases for find_first_greater_equal() and find_first_greater().
    [[nodiscard]] std::int32_t lower_bound_by_lex() const noexcept;
    [[nodiscard]] std::int32_t upper_bound_by_lex() const noexcept;

    //  Visit one or more slot categories.
    //
    //  For each visited slot, calls on_visit(slot_index, rank_index).
    //
    //  For multi-category visits, the order is lexed, then loose, then empty as applicable.
    //
    //  For lexed slots, rank_index increases monotonically in the range [0 : lexed_count()).
    //  For loose slots, rank_index increases monotonically in the range [lexed_count() : lexed_count() + loose_count()).
    //  For empty slots, rank_index increases monotonically in the range [lexed_count() + loose_count(), capacity()).
    void visit_occupied() noexcept;
    void visit_lexed() noexcept;
    void visit_loose() noexcept;
    void visit_empty() noexcept;
    void visit_all() noexcept;

    //  Tree shape queries.
    [[nodiscard]] std::uint32_t tree_height() const noexcept;
    [[nodiscard]] std::uint32_t tree_weight() const noexcept;

    //  validate_tree(check_lex_order) verifies AVL structure and balance.
    //  If check_lex_order is true, in-order ordering is validated via
    //  on_compare_keys() (nondecreasing, with stable order among equals).
    enum class LexCheck : std::int32_t { InOrder = 0, Unique = 1, None = 2 };
    [[nodiscard]] bool validate_tree(const LexCheck lex_check = LexCheck::None) const noexcept;

    //  check_integrity() validates metadata invariants, counts, list
    //  structure, tree balance, and index ranges. It does not validate
    //  lexical ordering.
    [[nodiscard]] bool check_integrity() const noexcept;

protected:

    //  Protected virtual functions represent the derived class responsibility interface.
    //
    //  The following functions are safe to call during these virtual function calls:
    //
    //      is_initialised(),
    //      capacity(), capacity_limit(), minimum_safe_capacity(),
    //      peak_usage(), peak_index(), high_index(), index_limit(),
    //      lexed_count(), loose_count(), empty_count(), occupied_count()
    //
    //  All other functions are unsafe when called from these virtual functions,
    //  and calling them will result in a soft-fail (hard-fail in debug).

    /// Visit callback for category traversal.
    ///
    /// For lexed slots the rank_index increases monotonically in the range [0 : lexed_count()).
    /// For loose slots the rank_index increases monotonically in the range [lexed_count() : lexed_count() + loose_count()).
    //  For empty slots the rank_index increases monotonically in the range [lexed_count() + loose_count(), capacity()).
    virtual void on_visit(const std::int32_t slot_index, const std::int32_t rank_index) noexcept
    {
        (void)slot_index;
        (void)rank_index;
    }

    /// Move a payload item between slots.
    ///
    /// Contract:
    ///   - source_index != target_index always
    ///   - exactly one of {source_index, target_index} may be -1
    ///   - -1 indicates temporary storage owned by the derived class
    ///
    /// This function is only called by sort_and_pack().
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

    /// Compare payload keys for two slots.
    ///
    /// Returns:
    ///   < 0  if key(source_index) < key(target_index)
    ///   = 0  if key(source_index) == key(target_index)
    ///   > 0  if key(source_index) > key(target_index)
    ///
    /// Special case:
    ///   source_index == -1 means "compare the current query/insert key against key(target_index)".
    virtual [[nodiscard]] std::int32_t on_compare_keys(const std::int32_t source_index, const std::int32_t target_index) const noexcept
    {
        (void)source_index;
        (void)target_index;
        return 0;
    }

private:

    //  Private virtual call re-entry guard structures and functions.

    enum class LockState : std::uint32_t { none = 0, on_visit, on_move_payload, on_reserve_empty, on_compare_keys };

    inline [[nodiscard]] bool is_safe(const bool allow_null = false) const noexcept;
    inline [[nodiscard]] bool lock(const LockState lock, const bool allow_null = false) const noexcept;
    inline void unlock(const LockState unlock) const noexcept;

    //  Private guarded virtual-call helpers.
    //  safe_on_visit() computes identifier from the visited slot category.
    //  safe_on_move_payload() is an available guarded wrapper but is not used by pack().
    //  Neither safe_on_visit() or safe_on_move_payload() are currently used, they are
    //  provided for symmetry and convenience only. Batched access is usually preferable.
    void safe_on_visit(const std::int32_t slot_index, const std::int32_t rank_index) noexcept;
    void safe_on_visit_dispatcher(const bool visit_lexed, const bool visit_loose, const bool visit_empty) noexcept;
    void safe_on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept;
    [[nodiscard]] std::uint32_t safe_on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept;
    [[nodiscard]] std::int32_t safe_on_compare_keys(const std::int32_t source_index, const std::int32_t target_index) const noexcept;
    [[nodiscard]] bool safe_has_duplicate_key(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool safe_has_duplicate_key_in_lexed(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] bool safe_has_duplicate_key_in_loose(const std::int32_t slot_index) const noexcept;

private:

    //  Private slot metadata structures.

    enum class SlotState : TMeta
    {
        is_unassigned = static_cast<TMeta>(~std::uint32_t(0) / 0x01u),   // 0xffffffff
        is_empty_slot = static_cast<TMeta>(~std::uint32_t(0) / 0x03u),   // 0x55555555
        is_loose_slot = static_cast<TMeta>(~std::uint32_t(0) / 0x05u),   // 0x33333333
        is_lexed_slot = static_cast<TMeta>(~std::uint32_t(0) / 0x0fu)    // 0x11111111
    };

    struct Slot
    {   //  Slot meta data structure
        TIndex  parent_index;
        TIndex  child_index[2];
        TMeta   balance_factor;
        TMeta   state;

        inline void set_slot_state(const SlotState slot_state) noexcept { state = static_cast<TMeta>(slot_state); }

        inline void set_is_unassigned() noexcept { set_slot_state(SlotState::is_unassigned); }
        inline void set_is_empty_slot() noexcept { set_slot_state(SlotState::is_empty_slot); }
        inline void set_is_loose_slot() noexcept { set_slot_state(SlotState::is_loose_slot); }
        inline void set_is_lexed_slot() noexcept { set_slot_state(SlotState::is_lexed_slot); }

        constexpr SlotState get_slot_state() const noexcept { return static_cast<SlotState>(state); }

        constexpr bool is_unassigned() const noexcept { return get_slot_state() == SlotState::is_unassigned; }
        constexpr bool is_empty_slot() const noexcept { return get_slot_state() == SlotState::is_empty_slot; }
        constexpr bool is_loose_slot() const noexcept { return get_slot_state() == SlotState::is_loose_slot; }
        constexpr bool is_lexed_slot() const noexcept { return get_slot_state() == SlotState::is_lexed_slot; }

        constexpr bool is_lexed_root() const noexcept { return parent_index < 0; }
        constexpr bool is_lexed_leaf() const noexcept { return (child_index[0] & child_index[1]) < 0; }
        constexpr bool is_lexed_twig() const noexcept { return (child_index[0] ^ child_index[1]) < 0; }
        constexpr bool is_lexed_stem() const noexcept { return (child_index[0] | child_index[1]) >= 0; }

        constexpr bool is_occupied() const noexcept
        {
            SlotState state = get_slot_state();
            return (state == SlotState::is_loose_slot) || (state == SlotState::is_lexed_slot);
        }

        constexpr std::uint32_t get_child_mask() const noexcept
        {
            return ((static_cast<std::uint32_t>(child_index[0]) >> 31) & 1u) | ((static_cast<std::uint32_t>(child_index[1]) >> 30) & 2u);
        }
    };

private:

    //  Private AVL management functions

    std::int32_t avl_single_rotate(const std::int32_t slot_index, const std::int32_t heavy_side) noexcept;

    std::int32_t avl_double_rotate(const std::int32_t slot_index, const std::int32_t heavy_side) noexcept;

    //  Insert a slot into the AVL tree (slot metadata only).
    //  key_index is forwarded as the 'source_index' operand to on_compare_keys() (may be -1 or a slot index).
    void avl_insert(const std::int32_t slot_index, const std::int32_t key_index) noexcept;

    //  Insert a slot into the AVL tree (slot metadata only).
    //  key_index == slot_index.
    void avl_insert(const std::int32_t slot_index) noexcept;

    //  Remove a slot from the AVL tree (slot metadata only).
    void avl_remove(const std::int32_t slot_index) noexcept;

private:

    //  Private implementation functions.

    //  Capacity growth recommendation.
    static inline std::uint32_t apply_growth_policy(const std::uint32_t capacity) noexcept;

    //  Tree validation functions.
    //  Returns height >= 0 on success; returns -1 on failure
    static inline [[nodiscard]] std::int32_t failed_validate_subtree() noexcept;
    [[nodiscard]] std::int32_t private_validate_subtree(const std::int32_t slot_index, const LexCheck lex_check = LexCheck::None) const noexcept;

    //  Integrity check functions.
    static inline [[nodiscard]] bool failed_integrity_check() noexcept;
    [[nodiscard]] bool private_integrity_check() const noexcept;

    //  Dispatcher for batched on-visit calls.
    void private_on_visit_dispatcher(const bool visit_lexed, const bool visit_loose, const bool visit_empty) noexcept;

    //  Resize the array capacity and apply any resulting required cleanup.
    [[nodiscard]] bool private_resize(const std::uint32_t requested_capacity) noexcept;

    //  Acquire an empty slot as either loose or lexed.
    //  Optionally reserve space to acquire the requested slot.
    //  Optionally verify that the requested slot key is unique.
    //  If slot_index is -1, acquires the first slot from the empty list.
    //  Returns the acquired slot index, or -1 on failure.
    [[nodiscard]] std::int32_t private_acquire(const std::int32_t slot_index, const bool lex, const bool require_unique, const bool allow_reserve) noexcept;

    //  Lexed index navigation.
    [[nodiscard]] std::int32_t private_prev_lexed(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::int32_t private_next_lexed(const std::int32_t slot_index) const noexcept;

    //  Duplicate key checking.
    [[nodiscard]] bool private_has_duplicate_key(const std::int32_t slot_index = -1) const noexcept;
    [[nodiscard]] bool private_has_duplicate_key_in_lexed(const std::int32_t slot_index = -1) const noexcept;
    [[nodiscard]] bool private_has_duplicate_key_in_loose(const std::int32_t slot_index = -1) const noexcept;

    //  Implementation of sort_and_pack().
    //  See the sort_and_pack() function prefix comments for details.
    void private_sort_and_compact(const bool use_external_payload = false) noexcept;

    //  Build a balanced subtree over an inclusive range range of slot indices.
    [[nodiscard]] std::int32_t build_balanced_subtree(const std::int32_t lower_index, const std::int32_t upper_index, const std::int32_t parent_index) noexcept;

    //  Convert the lexed tree structure into a ordered circular bi-directional list.
    //  using Slot::child_index[] as prev/next links. Returns the list head slot index.
    //  Returns the index of the list head.
    [[nodiscard]] std::int32_t lexed_to_list() noexcept;

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

    //  Set the parent_index of slots in a list to be an ordinal index
    void set_list_ordinals(const std::int32_t list_index, const std::uint32_t list_count, const std::int32_t ordinal_start) noexcept;

    //  Append a range of slot indices to the loose or empty list.
    //  The caller must ensure these slots are not currently actively managed.
    void append_range_to_loose_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept;
    void append_range_to_empty_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept;

    //  Internal helper functions for the move_to_* functions.
    void attach_to_lexed(const std::int32_t slot_index, const std::int32_t key_index) noexcept;
    void attach_to_lexed(const std::int32_t slot_index) noexcept;
    void attach_to_loose(const std::int32_t slot_index) noexcept;
    void attach_to_empty(const std::int32_t slot_index) noexcept;
    void remove_from_lexed(const std::int32_t slot_index) noexcept;
    void remove_from_loose(const std::int32_t slot_index) noexcept;
    void remove_from_empty(const std::int32_t slot_index) noexcept;

    //  Move a meta slot to a new meta category if not already a member of it.
    void move_to_lexed_tree(const std::int32_t slot_index, const std::int32_t key_index) noexcept;
    void move_to_lexed_tree(const std::int32_t slot_index) noexcept;
    void move_to_loose_list(const std::int32_t slot_index) noexcept;
    void move_to_empty_list(const std::int32_t slot_index) noexcept;

    //  Convert a slot index to its rank index.
    //  Returns the rank index for a slots.
    [[nodiscard]] std::int32_t convert_to_rank_index(const std::int32_t slot_index) const noexcept;

    //  Locate a slot index by its rank index.
    //  Returns the slot index or -1 if rank_index out of range.
    //  Valid ranks are in [0, capacity()).
    [[nodiscard]] std::int32_t locate_by_rank_index(const std::int32_t rank_index) const noexcept;

    //  Locate functions search the lexed tree using the current query key via on_compare_keys(-1, target).
    //  They return a slot index or -1 if not found.
    [[nodiscard]] std::int32_t locate_any_equal(const std::int32_t key_index = -1) const noexcept;
    [[nodiscard]] std::int32_t locate_first_equal(const std::int32_t key_index = -1) const noexcept;
    [[nodiscard]] std::int32_t locate_first_greater(const std::int32_t key_index = -1) const noexcept;
    [[nodiscard]] std::int32_t locate_first_greater_equal(const std::int32_t key_index = -1) const noexcept;
    [[nodiscard]] std::int32_t locate_last_equal(const std::int32_t key_index = -1) const noexcept;
    [[nodiscard]] std::int32_t locate_last_less(const std::int32_t key_index = -1) const noexcept;
    [[nodiscard]] std::int32_t locate_last_less_equal(const std::int32_t key_index = -1) const noexcept;

    //  Scan for lowest/highest occupied slot index in the slot metadata array.
    [[nodiscard]] std::int32_t min_occupied_index() const noexcept;
    [[nodiscard]] std::int32_t max_occupied_index() const noexcept;

    //  Diagnostics over slot metadata only.
    [[nodiscard]] std::uint32_t subtree_height(const std::int32_t slot_index) const noexcept;
    [[nodiscard]] std::uint32_t subtree_weight(const std::int32_t slot_index) const noexcept;

    //  These functions should only be called on construction or after a call to shutdown().
    bool move_from(TOrderedSlots& src) noexcept;
    bool copy_from(const TOrderedSlots& src) noexcept;
    void set_empty() noexcept;

private:

    //  Private data.
    std::uint32_t m_capacity = 0u;          //  allocated slot count
    std::uint32_t m_peak_usage = 0u;        //  peak occupied slot count (loose + lexed)
    std::int32_t  m_peak_index = -1;        //  peak occupied slot index
    std::int32_t  m_high_index = -1;        //  highest currently occupied index
    std::uint32_t m_lexed_count = 0u;       //  count of slots in the lexed tree
    std::uint32_t m_loose_count = 0u;       //  count of slots in the loose list
    std::uint32_t m_empty_count = 0u;       //  count of slots in the empty list
    std::int32_t  m_lexed_tree_root = -1;   //  index of the lexed slot tree root (or -1)
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
        "TOrderedSlots: Slot must be trivially copyable.");

    //  Enforce std::size_t has at least 32 bits
    static_assert(sizeof(std::size_t) >= sizeof(std::uint32_t),
        "TOrderedSlots: std::size_t must be at least 32 bits.");

    //  Enforce signed integer types so that negative sentinels and sign-based comparisons behave correctly
    static_assert(std::is_signed_v<TIndex> && std::is_signed_v<TMeta>,
        "TOrderedSlots: TIndex and TMeta must be signed integer types.");

    //  Enforce 2:1 index-to-metadata size ratio for predictable Slot layout and packing
    static_assert(sizeof(TMeta) * 2 == sizeof(TIndex),
        "TOrderedSlots: sizeof(TMeta) must be exactly half of sizeof(TIndex).");

    //  Enforce the only supported type pairs
    static_assert(
        (std::is_same_v<TIndex, std::int32_t> && std::is_same_v<TMeta, std::int16_t>) ||
        (std::is_same_v<TIndex, std::int16_t> && std::is_same_v<TMeta, std::int8_t>),
        "TOrderedSlots: Supported type pairs are (std::int32_t,std::int16_t) and (std::int16_t,std::int8_t).");

};

//! Protected function bodies

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::is_initialised() const noexcept
{
    return m_meta_slot_array.data() != nullptr;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::capacity() const noexcept
{
    return m_capacity;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::minimum_safe_capacity() const noexcept
{
    return static_cast<std::uint32_t>(m_high_index) + 1u;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::peak_usage() const noexcept
{
    return m_peak_usage;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::peak_index() const noexcept
{
    return m_peak_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::high_index() const noexcept
{
    return m_high_index;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::lexed_count() const noexcept
{
    return m_lexed_count;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::loose_count() const noexcept
{
    return m_loose_count;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::empty_count() const noexcept
{
    return m_empty_count;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::occupied_count() const noexcept
{
    return m_lexed_count + m_loose_count;
}

template<typename TIndex, typename TMeta>
inline TOrderedSlots<TIndex, TMeta>& TOrderedSlots<TIndex, TMeta>::operator=(TOrderedSlots&& src) noexcept
{
    (void)take(src);
    return *this;
}

template<typename TIndex, typename TMeta>
inline TOrderedSlots<TIndex, TMeta>& TOrderedSlots<TIndex, TMeta>::operator=(const TOrderedSlots& src) noexcept
{
    (void)clone(src);
    return *this;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::take(TOrderedSlots& src) noexcept
{
    return shutdown() ? move_from(src) : false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::clone(const TOrderedSlots& src) noexcept
{
    return shutdown() ? copy_from(src) : false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::clear() noexcept
{
    if (is_safe())
    {
        m_peak_usage = 0;
        m_peak_index = -1;
        m_high_index = -1;
        m_lexed_count = 0;
        m_loose_count = 0;
        m_empty_count = m_capacity;
        m_lexed_tree_root = -1;
        m_loose_list_head = -1;
        m_empty_list_head = range_to_list(0, static_cast<std::int32_t>(m_empty_count - 1), SlotState::is_empty_slot);
        m_lock = LockState::none;
        return true;
    }
    return false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::shutdown() noexcept
{
    if (is_safe(true))
    {
        m_meta_slot_array.deallocate();
        set_empty();
        return true;
    }
    return false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::initialise(const std::uint32_t capacity) noexcept
{
    return shutdown() ? private_resize(capacity) : false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::safe_resize(const std::uint32_t requested_capacity) noexcept
{
    return is_safe(true) ? private_resize(requested_capacity) : false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::reserve_empty(const std::uint32_t slot_count) noexcept
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
            std::uint32_t slot_limit = k_capacity_limit - m_lexed_count - m_loose_count;
            if (slot_limit >= slot_count)
            {
                std::uint32_t minimum_capacity = m_lexed_count + m_loose_count + slot_count;
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

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::shrink_to_fit() noexcept
{
    return (is_safe() && (m_high_index >= 0)) ? private_resize(static_cast<std::uint32_t>(m_high_index) + 1u) : false;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::acquire(const std::int32_t slot_index, const bool lex, const bool require_unique) noexcept
{
    return is_safe() ? private_acquire(slot_index, lex, require_unique, false) : -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::reserve_and_acquire(const std::int32_t slot_index, const bool lex, const bool require_unique) noexcept
{
    return is_safe() ? private_acquire(slot_index, lex, require_unique, true) : -1;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::erase(const std::int32_t slot_index) noexcept
{
    if (is_occupied(slot_index))
    {
        move_to_empty_list(slot_index);
        if (m_high_index == slot_index)
        {
            if ((m_lexed_count + m_loose_count) == 0)
            {
                m_high_index = -1;
            }
            else
            {
                const Slot* const meta = m_meta_slot_array.data();
                for (--m_high_index; !meta[m_high_index].is_occupied(); --m_high_index) {}
            }
        }
        return true;
    }
    return false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::is_occupied(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) && m_meta_slot_array.data()[slot_index].is_occupied();
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::is_safe_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe() && (static_cast<std::uint32_t>(slot_index) < m_capacity);
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::is_lexed_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) && m_meta_slot_array.data()[slot_index].is_lexed_slot();
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::is_loose_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) && m_meta_slot_array.data()[slot_index].is_loose_slot();
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::is_empty_slot(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) && m_meta_slot_array.data()[slot_index].is_empty_slot();
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::first_lexed() const noexcept
{
    std::int32_t first_index = -1;
    if (is_safe())
    {
        const Slot* const meta = m_meta_slot_array.data();
        for (std::int32_t scan_index = m_lexed_tree_root; scan_index >= 0; scan_index = meta[first_index].child_index[0]) first_index = scan_index;
    }
    return first_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::last_lexed() const noexcept
{
    std::int32_t last_index = -1;
    if (is_safe())
    {
        const Slot* const meta = m_meta_slot_array.data();
        for (std::int32_t scan_index = m_lexed_tree_root; scan_index >= 0; scan_index = meta[last_index].child_index[1]) last_index = scan_index;
    }
    return last_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::prev_lexed(const std::int32_t slot_index) const noexcept
{
    return is_lexed_slot(slot_index) ? private_prev_lexed(slot_index) : -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::next_lexed(const std::int32_t slot_index) const noexcept
{
    return is_lexed_slot(slot_index) ? private_next_lexed(slot_index) : -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::first_loose() const noexcept
{
    return is_safe() ? m_loose_list_head : -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::last_loose() const noexcept
{
    return (is_safe() && (m_loose_list_head != -1)) ? m_meta_slot_array.data()[m_loose_list_head].child_index[0] : -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::prev_loose(const std::int32_t slot_index) const noexcept
{
    if (is_loose_slot(slot_index) && (slot_index != m_loose_list_head))
    {
        return m_meta_slot_array.data()[slot_index].child_index[0];
    }
    return -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::next_loose(const std::int32_t slot_index) const noexcept
{
    if (is_loose_slot(slot_index))
    {
        const std::int32_t next_index = m_meta_slot_array.data()[slot_index].child_index[1];
        if (next_index != m_loose_list_head)
        {
            return next_index;
        }
    }
    return -1;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::has_duplicate_key(const std::int32_t slot_index) const noexcept
{
    return safe_has_duplicate_key(slot_index);
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::has_duplicate_key_in_lexed(const std::int32_t slot_index) const noexcept
{
    return safe_has_duplicate_key_in_lexed(slot_index);
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::has_duplicate_key_in_loose(const std::int32_t slot_index) const noexcept
{
    return safe_has_duplicate_key_in_loose(slot_index);
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::lex(const std::int32_t slot_index) noexcept
{
    if (is_loose_slot(slot_index) && lock(LockState::on_compare_keys))
    {
        move_to_lexed_tree(slot_index);
        unlock(LockState::on_compare_keys);
        return true;
    }
    return false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::unlex(const std::int32_t slot_index) noexcept
{
    if (is_lexed_slot(slot_index))
    {
        move_to_loose_list(slot_index);
        return true;
    }
    return false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::relex(const std::int32_t slot_index) noexcept
{
    if (is_lexed_slot(slot_index) && lock(LockState::on_compare_keys))
    {
        avl_remove(slot_index);
        Slot& slot = m_meta_slot_array.data()[slot_index];
        slot.parent_index = -1;
        slot.child_index[0] = slot.child_index[1] = -1;
        slot.balance_factor = 0;
        avl_insert(slot_index);
        unlock(LockState::on_compare_keys);
        return true;
    }
    return false;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::lex_all() noexcept
{
    if (lock(LockState::on_compare_keys) && (m_loose_count != 0))
    {
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t slot_index = m_loose_list_head;
        meta[meta[slot_index].child_index[0]].child_index[1] = -1;
        while (slot_index != -1)
        {
            Slot& slot = meta[slot_index];
            std::int32_t next_index = slot.child_index[1];
            slot.child_index[0] = slot.child_index[1] = -1;
            slot.set_is_lexed_slot();
            avl_insert(slot_index);
            slot_index = next_index;
        }
        m_loose_list_head = -1;
        m_lexed_count += m_loose_count;
        m_loose_count = 0;
        unlock(LockState::on_compare_keys);
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::unlex_all() noexcept
{
    if (is_safe() && (m_lexed_count != 0))
    {
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t lexed_list_head = lexed_to_list();
        std::int32_t slot_index = lexed_list_head;
        for (std::uint32_t slot_count = m_lexed_count; slot_count != 0; --slot_count)
        {
            Slot& slot = meta[slot_index];
            slot.set_is_loose_slot();
            slot_index = slot.child_index[1];
        }
        m_loose_list_head = combine_lists(m_loose_list_head, lexed_list_head);
        m_lexed_tree_root = -1;
        m_loose_count += m_lexed_count;
        m_lexed_count = 0;
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::relex_all() noexcept
{
    if (lock(LockState::on_compare_keys) && (m_lexed_count != 0))
    {
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t lexed_list_head = lexed_to_list();
        m_lexed_tree_root = -1;
        std::int32_t slot_index = lexed_list_head;
        for (std::uint32_t slot_count = m_lexed_count; slot_count != 0; --slot_count)
        {
            Slot& slot = meta[slot_index];
            std::int32_t next_index = slot.child_index[1];
            slot.child_index[0] = slot.child_index[1] = -1;
            avl_insert(slot_index);
            slot_index = next_index;
        }
        unlock(LockState::on_compare_keys);
    }
}

template<typename TIndex, typename TMeta>
[[nodiscard]] RankMap TOrderedSlots<TIndex, TMeta>::build_rank_map() const noexcept
{
    RankMap rank_map;
    if (is_safe() && (m_capacity != 0u))
    {
        if (rank_map.allocate(static_cast<std::size_t>(m_capacity)))
        {
            (void)rank_map.set_size(static_cast<std::size_t>(m_capacity));
            RankMapEntry* const map = rank_map.data();
            const Slot* const meta = m_meta_slot_array.data();
            std::int32_t rank_index = 0;
            if (m_lexed_count != 0)
            {
                std::int32_t slot_index = -1;
                for (std::int32_t left_index = m_lexed_tree_root; left_index >= 0; left_index = meta[slot_index = left_index].child_index[0]) {}
                while (slot_index >= 0)
                {
                    map[rank_index].rank_to_slot = slot_index;
                    map[slot_index].slot_to_rank = rank_index;
                    std::int32_t from_index = meta[slot_index].child_index[1];
                    if (from_index < 0)
                    {
                        for (from_index = slot_index; (((slot_index = meta[from_index].parent_index) >= 0) && (meta[slot_index].child_index[0] != from_index)); from_index = slot_index) {}
                    }
                    else
                    {
                        for (slot_index = from_index; ((from_index = meta[slot_index].child_index[0]) >= 0); slot_index = from_index) {}
                    }
                    ++rank_index;
                }
            }
            if (m_loose_count != 0)
            {
                std::int32_t slot_index = m_loose_list_head;
                for (std::uint32_t loose_count = m_loose_count; loose_count != 0; --loose_count)
                {
                    map[rank_index].rank_to_slot = slot_index;
                    map[slot_index].slot_to_rank = rank_index;
                    slot_index = meta[slot_index].child_index[1];
                    ++rank_index;
                }
            }
            if (m_empty_count != 0)
            {
                std::int32_t slot_index = m_empty_list_head;
                for (std::uint32_t empty_count = m_empty_count; empty_count != 0; --empty_count)
                {
                    map[rank_index].rank_to_slot = slot_index;
                    map[slot_index].slot_to_rank = rank_index;
                    slot_index = meta[slot_index].child_index[1];
                    ++rank_index;
                }
            }
            MV_HARD_ASSERT(rank_index == static_cast<std::int32_t>(m_capacity));
        }
    }
    return rank_map;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::sort_and_pack(const bool use_external_payload) noexcept
{
    if (lock(LockState::on_move_payload))
    {
        private_sort_and_compact(use_external_payload);
        unlock(LockState::on_move_payload);
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::rebuild_loose_in_index_order() noexcept
{
    if (is_safe() && (m_loose_count != 0))
    {
        m_loose_list_head = state_to_list(0, m_high_index, SlotState::is_loose_slot);
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::rebuild_empty_in_index_order() noexcept
{
    if (is_safe() && (m_empty_count != 0))
    {
        m_empty_list_head = combine_lists(
            state_to_list(0, m_high_index, SlotState::is_empty_slot),
            range_to_list((m_high_index + 1), static_cast<std::int32_t>(m_capacity - 1u), SlotState::is_empty_slot));
    }
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::rank_index_of(const std::int32_t slot_index) const noexcept
{
    return is_safe_slot(slot_index) ? convert_to_rank_index(slot_index) : -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_by_rank_index(const std::int32_t rank_index) const noexcept
{
    return is_safe() ? locate_by_rank_index(rank_index) : -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_any_equal() const noexcept
{
    std::int32_t slot_index = -1;
    if (lock(LockState::on_compare_keys))
    {
        slot_index = locate_any_equal();
        unlock(LockState::on_compare_keys);
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_first_equal() const noexcept
{
    std::int32_t slot_index = -1;
    if (lock(LockState::on_compare_keys))
    {
        slot_index = locate_first_equal();
        unlock(LockState::on_compare_keys);
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_first_greater() const noexcept
{
    std::int32_t slot_index = -1;
    if (lock(LockState::on_compare_keys))
    {
        slot_index = locate_first_greater();
        unlock(LockState::on_compare_keys);
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_first_greater_equal() const noexcept
{
    std::int32_t slot_index = -1;
    if (lock(LockState::on_compare_keys))
    {
        slot_index = locate_first_greater_equal();
        unlock(LockState::on_compare_keys);
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_last_equal() const noexcept
{
    std::int32_t slot_index = -1;
    if (lock(LockState::on_compare_keys))
    {
        slot_index = locate_last_equal();
        unlock(LockState::on_compare_keys);
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_last_less() const noexcept
{
    std::int32_t slot_index = -1;
    if (lock(LockState::on_compare_keys))
    {
        slot_index = locate_last_less();
        unlock(LockState::on_compare_keys);
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::find_last_less_equal() const noexcept
{
    std::int32_t slot_index = -1;
    if (lock(LockState::on_compare_keys))
    {
        slot_index = locate_last_less_equal();
        unlock(LockState::on_compare_keys);
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::lower_bound_by_lex() const noexcept
{
    return find_first_greater_equal();
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::upper_bound_by_lex() const noexcept
{
    return find_first_greater();
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::visit_occupied() noexcept
{
    safe_on_visit_dispatcher(true, true, false);
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::visit_lexed() noexcept
{
    safe_on_visit_dispatcher(true, false, false);
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::visit_loose() noexcept
{
    safe_on_visit_dispatcher(false, true, false);
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::visit_empty() noexcept
{
    safe_on_visit_dispatcher(false, false, true);
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::visit_all() noexcept
{
    safe_on_visit_dispatcher(true, true, true);
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::tree_height() const noexcept
{
    return is_safe() ? subtree_height(m_lexed_tree_root) : 0;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::tree_weight() const noexcept
{
    return is_safe() ? subtree_weight(m_lexed_tree_root) : 0;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::validate_tree(const LexCheck lex_check) const noexcept
{
    bool valid = false;
    if (is_safe())
    {
        if (m_lexed_count == 0)
        {
            valid = m_lexed_tree_root == -1;
        }
        else if (lex_check != LexCheck::None)
        {
            if (lock(LockState::on_compare_keys))
            {
                valid = private_validate_subtree(m_lexed_tree_root, lex_check) > 0;
                unlock(LockState::on_compare_keys);
            }
        }
        else
        {
            valid = private_validate_subtree(m_lexed_tree_root, lex_check) > 0;
        }
    }
    return valid;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::check_integrity() const noexcept
{
    return is_safe() ? private_integrity_check() : false;
}

//! Private function bodies

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::is_safe(const bool allow_null) const noexcept
{
    return MV_FAIL_SAFE_ASSERT(m_lock == LockState::none) && (allow_null || (m_meta_slot_array.data() != nullptr));
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::lock(const LockState lock, const bool allow_null) const noexcept
{
    bool success = false;
    if (MV_FAIL_SAFE_ASSERT(m_lock == LockState::none) && (allow_null || (m_meta_slot_array.data() != nullptr)))
    {
        m_lock = lock;
        success = true;
    }
    return success;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::unlock(const LockState unlock) const noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_lock == unlock))
    {
        m_lock = LockState::none;
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::safe_on_visit(const std::int32_t slot_index, const std::int32_t rank_index) noexcept
{
    if (lock(LockState::on_visit))
    {
        on_visit(slot_index, rank_index);
        unlock(LockState::on_visit);
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::safe_on_visit_dispatcher(const bool visit_lexed, const bool visit_loose, const bool visit_empty) noexcept
{
    if (lock(LockState::on_visit))
    {
        private_on_visit_dispatcher(visit_lexed, visit_loose, visit_empty);
        unlock(LockState::on_visit);
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::safe_on_move_payload(const std::int32_t source_index, const std::int32_t target_index) noexcept
{
    if (lock(LockState::on_move_payload))
    {
        on_move_payload(source_index, target_index);
        unlock(LockState::on_move_payload);
    }
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::safe_on_reserve_empty(const std::uint32_t minimum_capacity, const std::uint32_t recommended_capacity) noexcept
{
    std::uint32_t reserve_capacity = 0u;
    if (lock(LockState::on_reserve_empty))
    {
        reserve_capacity = on_reserve_empty(minimum_capacity, recommended_capacity);
        unlock(LockState::on_reserve_empty);
    }
    return reserve_capacity;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::safe_on_compare_keys(const std::int32_t source_index, const std::int32_t target_index) const noexcept
{
    std::int32_t relationship = 0;
    if (lock(LockState::on_compare_keys))
    {
        relationship = on_compare_keys(source_index, target_index);
        unlock(LockState::on_compare_keys);
    }
    return relationship;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::safe_has_duplicate_key(const std::int32_t slot_index) const noexcept
{
    bool has_duplicate = false;
    if (lock(LockState::on_compare_keys))
    {
        has_duplicate = private_has_duplicate_key(slot_index);
        unlock(LockState::on_compare_keys);
    }
    return has_duplicate;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::safe_has_duplicate_key_in_lexed(const std::int32_t slot_index) const noexcept
{
    bool has_duplicate = false;
    if (lock(LockState::on_compare_keys))
    {
        has_duplicate = private_has_duplicate_key_in_lexed(slot_index);
        unlock(LockState::on_compare_keys);
    }
    return has_duplicate;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::safe_has_duplicate_key_in_loose(const std::int32_t slot_index) const noexcept
{
    bool has_duplicate = false;
    if (lock(LockState::on_compare_keys))
    {
        has_duplicate = private_has_duplicate_key_in_loose(slot_index);
        unlock(LockState::on_compare_keys);
    }
    return has_duplicate;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::avl_single_rotate(const std::int32_t slot_index, const std::int32_t heavy_side) noexcept
{
    Slot* const meta = m_meta_slot_array.data();

    const std::int32_t light_side = heavy_side ^ 1;

    Slot& slot = meta[slot_index];
    const std::int32_t parent_index = slot.parent_index;

    const std::int32_t child_index = slot.child_index[heavy_side];
    Slot& child = meta[child_index];

    const std::int32_t light_child_index = child.child_index[light_side];

    slot.child_index[heavy_side] = light_child_index;
    if (light_child_index >= 0)
    {
        meta[light_child_index].parent_index = slot_index;
    }

    child.child_index[light_side] = slot_index;
    slot.parent_index = child_index;

    child.parent_index = parent_index;
    if (parent_index >= 0)
    {
        Slot& parent = meta[parent_index];
        const std::int32_t parent_side = (parent.child_index[1] == slot_index) ? 1 : 0;
        parent.child_index[parent_side] = child_index;
    }
    else
    {
        m_lexed_tree_root = child_index;
    }

    return child_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::avl_double_rotate(const std::int32_t slot_index, const std::int32_t heavy_side) noexcept
{
    Slot* const meta = m_meta_slot_array.data();

    const std::int32_t light_side = heavy_side ^ 1;

    Slot& slot = meta[slot_index];
    const std::int32_t parent_index = slot.parent_index;

    const std::int32_t child_index = slot.child_index[heavy_side];
    Slot& child = meta[child_index];

    const std::int32_t grandchild_index = child.child_index[light_side];
    Slot& grandchild = meta[grandchild_index];

    const std::int32_t heavy_grandchild_index = grandchild.child_index[heavy_side];
    const std::int32_t light_grandchild_index = grandchild.child_index[light_side];

    //  detach grandchild's heavy subtree and attach it as child's light subtree.
    child.child_index[light_side] = heavy_grandchild_index;
    if (heavy_grandchild_index >= 0)
    {
        meta[heavy_grandchild_index].parent_index = child_index;
    }

    //  detach grandchild's light subtree and attach it as slot's heavy subtree.
    slot.child_index[heavy_side] = light_grandchild_index;
    if (light_grandchild_index >= 0)
    {
        meta[light_grandchild_index].parent_index = slot_index;
    }

    //  promote grandchild above child and slot.
    grandchild.child_index[heavy_side] = child_index;
    child.parent_index = grandchild_index;

    grandchild.child_index[light_side] = slot_index;
    slot.parent_index = grandchild_index;

    //  attach promoted subtree to old parent (or become root).
    grandchild.parent_index = parent_index;
    if (parent_index >= 0)
    {
        Slot& parent = meta[parent_index];
        const std::int32_t parent_side = (parent.child_index[1] == slot_index) ? 1 : 0;
        parent.child_index[parent_side] = grandchild_index;
    }
    else
    {
        m_lexed_tree_root = grandchild_index;
    }

    //  balance-factor updates.
    const std::int32_t grandchild_bf = grandchild.balance_factor;
    grandchild.balance_factor = 0;

    if (grandchild_bf == 1)
    {
        if (heavy_side == 1)
        {
            slot.balance_factor = -1;
            child.balance_factor = 0;
        }
        else
        {
            slot.balance_factor = 0;
            child.balance_factor = -1;
        }
    }
    else if (grandchild_bf == -1)
    {
        if (heavy_side == 1)
        {
            slot.balance_factor = 0;
            child.balance_factor = 1;
        }
        else
        {
            slot.balance_factor = 1;
            child.balance_factor = 0;
        }
    }
    else
    {
        slot.balance_factor = 0;
        child.balance_factor = 0;
    }

    return grandchild_index;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::avl_insert(const std::int32_t slot_index, const std::int32_t key_index) noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);

    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    slot.parent_index = -1;
    slot.child_index[0] = -1;
    slot.child_index[1] = -1;
    slot.balance_factor = 0;
    slot.set_is_lexed_slot();

    if (m_lexed_tree_root < 0)
    {
        m_lexed_tree_root = slot_index;
    }
    else
    {
        std::int32_t walk_index = -1;
        std::int32_t walk_side = 0;

        for (std::int32_t scan_index = m_lexed_tree_root; scan_index >= 0; scan_index = meta[walk_index].child_index[walk_side])
        {
            walk_index = scan_index;
            walk_side = (on_compare_keys(key_index, walk_index) >= 0) ? 1 : 0;
        }
        meta[walk_index].child_index[walk_side] = slot_index;
        slot.parent_index = walk_index;

        //  walk_side: 0/1, delta: -1/+1
        std::int32_t delta = (walk_side << 1) - 1;
        while (walk_index >= 0)
        {
            Slot& walk = meta[walk_index];
            walk.balance_factor += delta;
            if (walk.balance_factor == delta)
            {   //  walk.balance_factor is 1 or -1
                const std::int32_t walk_parent_index = walk.parent_index;
                delta = ((walk_parent_index >= 0) && (meta[walk_parent_index].child_index[1] == walk_index)) ? 1 : -1;
                walk_index = walk_parent_index;
            }
            else
            {   //  walk.balance_factor is 0 or 2 or -2
                if (walk.balance_factor != 0)
                {
                    const std::int32_t heavy_side = (walk.balance_factor > 0) ? 1 : 0;
                    const std::int32_t expected_child_balance_factor = (heavy_side << 1) - 1;

                    Slot& child = meta[walk.child_index[heavy_side]];
                    const std::int32_t child_balance_factor = child.balance_factor;

                    if (child_balance_factor == expected_child_balance_factor)
                    {
                        walk_index = avl_single_rotate(walk_index, heavy_side);
                        walk.balance_factor = 0;
                        meta[walk_index].balance_factor = 0;
                    }
                    else
                    {
                        (void)avl_double_rotate(walk_index, heavy_side);
                    }
                }
                break;
            }
        }
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::avl_insert(const std::int32_t slot_index) noexcept
{
    avl_insert(slot_index, slot_index);
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::avl_remove(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];

    const std::int32_t parent_index = slot.parent_index;
    const std::int32_t parent_side =
        ((parent_index >= 0) && (meta[parent_index].child_index[1] == slot_index)) ? 1 : 0;

    std::int32_t walk_index = -1;
    std::int32_t walk_side = 0;

    if (slot.is_lexed_stem())
    {
        std::int32_t successor_index = slot.child_index[1];
        while (meta[successor_index].child_index[0] >= 0)
        {
            successor_index = meta[successor_index].child_index[0];
        }

        Slot& successor = meta[successor_index];
        const std::int32_t successor_parent_index = successor.parent_index;
        const std::int32_t successor_right_index = successor.child_index[1];

        if (successor_parent_index != slot_index)
        {
            Slot& successor_parent = meta[successor_parent_index];
            successor_parent.child_index[0] = successor_right_index;
            if (successor_right_index >= 0)
            {
                meta[successor_right_index].parent_index = successor_parent_index;
            }

            successor.child_index[1] = slot.child_index[1];
            meta[successor.child_index[1]].parent_index = successor_index;

            walk_index = successor_parent_index;
            walk_side = 0;
        }
        else
        {
            // successor was slot.right (direct child), so right height under successor shrank
            walk_index = successor_index;
            walk_side = 1;
        }

        successor.child_index[0] = slot.child_index[0];
        meta[successor.child_index[0]].parent_index = successor_index;

        successor.parent_index = parent_index;
        if (parent_index >= 0)
        {
            meta[parent_index].child_index[parent_side] = successor_index;
        }
        else
        {
            m_lexed_tree_root = successor_index;
        }

        successor.balance_factor = slot.balance_factor;
    }
    else
    {
        const std::int32_t child_side = (slot.child_index[0] >= 0) ? 0 : 1;
        const std::int32_t child_index = slot.child_index[child_side];

        if (parent_index >= 0)
        {
            meta[parent_index].child_index[parent_side] = child_index;
        }
        else
        {
            m_lexed_tree_root = child_index;
        }

        if (child_index >= 0)
        {
            meta[child_index].parent_index = parent_index;
        }

        walk_index = parent_index;
        walk_side = parent_side;
    }

    //  walk_side: 0/1, delta: -1/+1.
    std::int32_t delta = (walk_side << 1) - 1;
    while (walk_index >= 0)
    {
        Slot& walk = meta[walk_index];

        //  subtree parent is not changed by rotation, but
        //  walk.parent_index may change, so grab it now.
        const std::int32_t walk_parent_index = walk.parent_index;

        walk.balance_factor -= delta;
        if (walk.balance_factor != 0)
        {
            if ((walk.balance_factor == 1) || (walk.balance_factor == -1))
            {
                break;
            }
    
            const std::int32_t heavy_side = (walk.balance_factor > 0) ? 1 : 0;
            const std::int32_t expected_child_balance_factor = (heavy_side << 1) - 1;
    
            Slot& child = meta[walk.child_index[heavy_side]];
            const std::int32_t child_balance_factor = child.balance_factor;
    
            if ((child_balance_factor == 0) || (child_balance_factor == expected_child_balance_factor))
            {
                const std::int32_t rebalance_factor = expected_child_balance_factor - child_balance_factor;
                walk_index = avl_single_rotate(walk_index, heavy_side);
                walk.balance_factor = rebalance_factor;
                meta[walk_index].balance_factor = -rebalance_factor;
                if (child_balance_factor == 0)
                {
                    break;
                }
            }
            else
            {
                walk_index = avl_double_rotate(walk_index, heavy_side);
                if (meta[walk_index].balance_factor != 0)
                {
                    break;
                }
            }
        }
    
        delta = ((walk_parent_index >= 0) && (meta[walk_parent_index].child_index[1] == walk_index)) ? 1 : -1;
        walk_index = walk_parent_index;
    }

    slot.parent_index = -1;
    slot.child_index[0] = -1;
    slot.child_index[1] = -1;
    slot.balance_factor = 0;
    slot.set_is_unassigned();
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::apply_growth_policy(const std::uint32_t capacity) noexcept
{
    return static_cast<std::uint32_t>(std::min(memory::vector_growth_policy(static_cast<std::size_t>(capacity)), static_cast<std::size_t>(k_capacity_limit)));
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::failed_validate_subtree() noexcept
{
    MV_HARD_ASSERT(false);
    return -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::private_validate_subtree(const std::int32_t slot_index, const LexCheck lex_check) const noexcept
{
    if (static_cast<std::uint32_t>(slot_index) >= m_capacity)
    {   //  early out on an invalid slot_index
        return failed_validate_subtree();
    }

    const Slot* const meta = m_meta_slot_array.data();
    const Slot& slot = meta[slot_index];

    if (!slot.is_lexed_slot())
    {   //  early out on link to a slot that is not in the tree
        return failed_validate_subtree();
    }

    if ((slot.balance_factor < -1) || (slot.balance_factor > 1))
    {   //  early out on balance out of range
        return failed_validate_subtree();
    }

    const std::int32_t index_l = slot.child_index[0];
    const std::int32_t index_r = slot.child_index[1];

    if ((static_cast<std::uint32_t>(index_l) + 1u) > m_capacity)
    {   //  early out on invalid index_l
        return failed_validate_subtree();
    }
    if ((static_cast<std::uint32_t>(index_r) + 1u) > m_capacity)
    {   //  early out on invalid index_r
        return failed_validate_subtree();
    }

    std::int32_t height_l = 0;
    if (index_l >= 0)
    {
        if (meta[index_l].parent_index != slot_index)
        {   //  early out on left child parent_index mismatch
            return failed_validate_subtree();
        }
        height_l = private_validate_subtree(index_l, lex_check);
        if (height_l < 0)
        {   //  propagate failure upwards
            return failed_validate_subtree();
        }
    }

    std::int32_t height_r = 0;
    if (index_r >= 0)
    {
        if (meta[index_r].parent_index != slot_index)
        {   //  early out on right child parent_index mismatch
            return failed_validate_subtree();
        }
        height_r = private_validate_subtree(index_r, lex_check);
        if (height_r < 0)
        {   //  propagate failure upwards
            return failed_validate_subtree();
        }
    }

    const std::int32_t balance_factor = height_r - height_l;

    if (static_cast<std::int32_t>(slot.balance_factor) != balance_factor)
    {   //  early out on mismatched balance
        return failed_validate_subtree();
    }

    if (lex_check != LexCheck::None)
    {
        MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
        const std::int32_t unique_bias = static_cast<std::int32_t>(lex_check);

        std::int32_t prev_index = private_prev_lexed(slot_index);
        if (prev_index >= 0)
        {
            if (private_next_lexed(prev_index) != slot_index)
            {
                return failed_validate_subtree();
            }
            if ((on_compare_keys(slot_index, prev_index) - unique_bias) < 0)
            {
                return failed_validate_subtree();
            }
        }

        std::int32_t next_index = private_next_lexed(slot_index);
        if (next_index >= 0)
        {
            if (private_prev_lexed(next_index) != slot_index)
            {
                return failed_validate_subtree();
            }
            if ((on_compare_keys(slot_index, next_index) + unique_bias) > 0)
            {
                return failed_validate_subtree();
            }
        }
    }

    return std::max(height_l, height_r) + 1;
}

//  This function only exists as a debug convenience to help capture integrity check failure causes.
//  It may be expanded on in the future as a potential logging site.
template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::failed_integrity_check() noexcept
{
    MV_HARD_ASSERT(false);
    return false;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::private_integrity_check() const noexcept
{
    const Slot* const meta = m_meta_slot_array.data();
    if (meta != nullptr)
    {
        if ((std::uintptr_t(meta) % alignof(Slot)) != 0u)
        {   //  basic slot array alignment check failed
            return failed_integrity_check();
        }

        if ((m_capacity == 0) || (m_capacity > k_capacity_limit) ||
            ((m_lexed_count + m_loose_count + m_empty_count) != m_capacity) ||
            (m_lexed_count > m_capacity) || (m_loose_count > m_capacity) || (m_empty_count > m_capacity))
        {   //  basic capacity integrity test failed
            return failed_integrity_check();
        }

        if (((static_cast<std::uint32_t>(m_lexed_tree_root) + 1u) > m_capacity) || ((m_lexed_count == 0u) ? (m_lexed_tree_root != -1) : (m_lexed_tree_root == -1)))
        {   //  basic lexed tree root index integrity test failed
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

        if ((m_high_index < -1) || ((m_high_index == -1) ? ((m_lexed_count + m_loose_count) != 0u) : ((static_cast<std::uint32_t>(m_high_index) + 1u) < (m_lexed_count + m_loose_count))))
        {   //  basic high index integrity test failed
            return failed_integrity_check();
        }

        if ((m_peak_index < -1) || (m_peak_usage > k_capacity_limit) || (m_peak_usage > (static_cast<std::uint32_t>(m_peak_index) + 1u)))
        {   //  basic peak usage and peak index integrity test failed
            return failed_integrity_check();
        }

        std::uint32_t lexed_count = 0;
        std::uint32_t loose_count = 0;
        std::uint32_t empty_count = 0;
        for (std::int32_t slot_index = static_cast<std::int32_t>(m_capacity - 1u); slot_index >= 0; --slot_index)
        {   //  basic array integrity check
            const Slot& slot = meta[slot_index];
            switch (slot.get_slot_state())
            {
                case (SlotState::is_lexed_slot):
                {
                    ++lexed_count;
                    if (((static_cast<std::uint32_t>(slot.parent_index) + 1u) > m_capacity) || ((slot.parent_index == -1) && (m_lexed_tree_root != slot_index)))
                    {   //  parent index is invalid
                        return failed_integrity_check();
                    }
                    if (((static_cast<std::uint32_t>(slot.child_index[0]) + 1u) > m_capacity) || ((static_cast<std::uint32_t>(slot.child_index[1]) + 1u) > m_capacity))
                    {   //  child index is invalid
                        return failed_integrity_check();
                    }
                    if ((slot_index == slot.parent_index) || (slot_index == slot.child_index[0]) || (slot_index == slot.child_index[1]))
                    {   //  invalid: an index in this slot reference this slot
                        return failed_integrity_check();
                    }
                    if ((slot.parent_index != -1) && ((slot.parent_index == slot.child_index[0]) || (slot.parent_index == slot.child_index[1])))
                    {   //  invalid: children reference the parent slot or vice-versa
                        return failed_integrity_check();
                    }
                    if ((slot.parent_index != -1) && !meta[slot.parent_index].is_lexed_slot())
                    {   //  invalid: parent index references outside of the tree
                        return failed_integrity_check();
                    }
                    if ((slot.child_index[0] != -1) && !meta[slot.child_index[0]].is_lexed_slot())
                    {   //  invalid: child index references outside of the tree
                        return failed_integrity_check();
                    }
                    if ((slot.child_index[1] != -1) && !meta[slot.child_index[1]].is_lexed_slot())
                    {   //  invalid: child index references outside of the tree
                        return failed_integrity_check();
                    }
                    if (slot.parent_index != -1)
                    {   //  not a root node
                        const Slot& parent_slot = meta[slot.parent_index];
                        if ((slot_index != parent_slot.child_index[0]) && (slot_index != parent_slot.child_index[1]))
                        {   //  invalid: this node is not a child of its parent node
                            return failed_integrity_check();
                        }
                    }
                    if (slot.child_index[0] == slot.child_index[1])
                    {   //  only leaves can have matching child indices
                        if (slot.child_index[0] != -1)
                        {   //  invalid: not a leaf
                            return failed_integrity_check();
                        }
                        if (slot.balance_factor != 0)
                        {   //  invalid: leaf nodes must be balanced by definition
                            return failed_integrity_check();
                        }
                    }
                    else
                    {
                        if (slot.child_index[0] == -1)
                        {
                            if (slot.balance_factor != 1)
                            {   //  invalid: right child only branches must be balanced to the right
                                return failed_integrity_check();
                            }
                        }
                        else if (slot_index != meta[slot.child_index[0]].parent_index)
                        {   //  invalid: the left child is not parented to this slot
                            return failed_integrity_check();
                        }
                        if (slot.child_index[1] == -1)
                        {
                            if (slot.balance_factor != -1)
                            {   //  invalid: left child only branches must be balanced to the left
                                return failed_integrity_check();
                            }
                        }
                        else if (slot_index != meta[slot.child_index[1]].parent_index)
                        {   //  invalid: the right child is not parented to this slot
                            return failed_integrity_check();
                        }
                        if ((slot.balance_factor < -1) || (slot.balance_factor > 1))
                        {   //  balance is invalid
                            return failed_integrity_check();
                        }
                    }
                    break;
                }
                case (SlotState::is_loose_slot):
                case (SlotState::is_empty_slot):
                {
                    if (slot.is_loose_slot())
                    {
                        ++loose_count;
                    }
                    else
                    {
                        ++empty_count;
                    }
                    if ((slot.parent_index != -1) || (slot.balance_factor != 0))
                    {   //  invalid: invariants check failed
                        return failed_integrity_check();
                    }
                    if ((static_cast<std::uint32_t>(slot.child_index[0]) >= m_capacity) || (static_cast<std::uint32_t>(slot.child_index[1]) >= m_capacity))
                    {   //  loose or empty list index is invalid
                        return failed_integrity_check();
                    }
                    break;
                }
                default:
                {   //  slot state is invalid
                    return failed_integrity_check();
                }
            }
        }
        if (lexed_count != m_lexed_count)
        {   //  the lexed count is invalid
            return failed_integrity_check();
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
                if (meta[slot.child_index[1]].child_index[0] != empty_index)
                {   //  bi-directional linkage is broken
                    return failed_integrity_check();
                }
                empty_index = slot.child_index[1];
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
                if (meta[slot.child_index[1]].child_index[0] != loose_index)
                {   //  bi-directional linkage is broken
                    return failed_integrity_check();
                }
                loose_index = slot.child_index[1];
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
        if (lexed_count != 0)
        {   //  validate the tree and balance

            if (meta[m_lexed_tree_root].parent_index != -1)
            {   //  the root is invalid
                return failed_integrity_check();
            }

            //  step_cap = 2*N - 1 node-visits for Euler tour.
            //  N is capped at 2^31, so 2*N-1 fits in std::uint32_t (== 0xFFFFFFFF at the max).
            std::uint32_t step_cap = (lexed_count << 1) - 1;

            //  for an AVL the theoretical depth limit is ~ 1.44 * log2(n) + 1
            //
            //  the depth_cap calculation:
            //      effectively 1.5 * (floor(log2(n)) + 1)
            //      exceeds the theoretical limit allowing some margin
            //      max depth_cap = 48 (based on max n of 2^31)
            // 
            //  depth:
            //      at root = 0
            //      incremented on descent
            //      used as an index into height_cache
            //      check must be depth < depth_cap after increment
            std::uint32_t depth_cap = 0;
            for (std::uint32_t check = lexed_count; check; check >>= 1)
            {
                ++depth_cap;
            }
            depth_cap += (depth_cap >> 1);
            std::int8_t height_cache[48];    //  max depth_cap capacity

            std::uint32_t depth = 0;
            std::int32_t height = 0;
            std::int32_t step_index = -1;
            std::int32_t from_index = -1;
            std::int32_t scan_index = m_lexed_tree_root;
            while (scan_index >= 0)
            {
                const Slot& slot = meta[scan_index];

                if (from_index == slot.parent_index)
                {   //  came from parent
                    height_cache[depth] = 0;
                    height = 0;
                }
                else if (from_index == slot.child_index[0])
                {   //  came from left child
                    height_cache[depth] = static_cast<std::int8_t>(height);
                    height = 0;
                }

                if ((from_index == slot.parent_index) && (slot.child_index[0] != -1))
                {   //  descend to left child
                    step_index = slot.child_index[0];

                    ++depth;
                    if (depth >= depth_cap)
                    {   //  too deeply nested to be a valid AVL tree and would exceed height_cache capacity
                        return failed_integrity_check();
                    }
                }
                else if ((from_index != slot.child_index[1]) && (slot.child_index[1] != -1))
                {   //  descend to right child
                    step_index = slot.child_index[1];

                    ++depth;
                    if (depth >= depth_cap)
                    {   //  too deeply nested to be a valid AVL tree and would exceed height_cache capacity
                        return failed_integrity_check();
                    }
                }
                else
                {   //  ascend to parent
                    step_index = slot.parent_index;

                    if (step_index != -1)
                    {
                        std::int32_t other_height = static_cast<std::int32_t>(height_cache[depth]);
                        std::int32_t balance_factor = height - other_height;
                        if (static_cast<std::int32_t>(slot.balance_factor) != balance_factor)
                        {   //  there is a tree balance mismatch
                            return failed_integrity_check();
                        }
                        height = std::max(height, other_height) + 1;

                        if (depth == 0)
                        {   //  we should be exiting the tree but are not
                            return failed_integrity_check();
                        }
                        --depth;
                    }

                    if (lexed_count == 0)
                    {   //  too many slots for this to be a valid tree
                        return failed_integrity_check();
                    }
                    --lexed_count;
                }

                from_index = scan_index;
                scan_index = step_index;

                if (step_cap == 0)
                {   //  too many steps for this to be a valid tree
                    return failed_integrity_check();
                }
                --step_cap;
            }
            if (lexed_count > 0)
            {   //  the tree does not contain all the lexed slots
                return failed_integrity_check();
            }
        }
    }
    else
    {
        if ((m_capacity != 0) || (m_peak_usage != 0) ||
            (m_peak_index != -1) || (m_high_index != -1) ||
            (m_lexed_count != 0) || (m_lexed_tree_root != -1) ||
            (m_loose_count != 0) || (m_loose_list_head != -1) ||
            (m_empty_count != 0) || (m_empty_list_head != -1))
        {
            return failed_integrity_check();
        }
    }
    return true;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::private_on_visit_dispatcher(const bool visit_lexed, const bool visit_loose, const bool visit_empty) noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_visit);
    const Slot* const meta = m_meta_slot_array.data();
    if (visit_lexed)
    {
        std::int32_t slot_index = m_lexed_tree_root;
        for (std::int32_t to_index = slot_index; to_index >= 0; to_index = meta[slot_index].child_index[0]) slot_index = to_index;
        for (std::int32_t rank_index = 0; slot_index >= 0; ++rank_index)
        {
            on_visit(slot_index, rank_index);
            std::int32_t from_index = meta[slot_index].child_index[1];
            if (from_index < 0)
            {
                for (from_index = slot_index; (((slot_index = meta[from_index].parent_index) >= 0) && (meta[slot_index].child_index[0] != from_index)); from_index = slot_index) {}
            }
            else
            {
                for (slot_index = from_index; ((from_index = meta[slot_index].child_index[0]) >= 0); slot_index = from_index) {}
            }
        }
    }
    if (visit_loose)
    {
        std::int32_t rank_index = static_cast<std::int32_t>(m_lexed_count);
        std::int32_t slot_index = m_loose_list_head;
        for (std::uint32_t loose_count = m_loose_count; loose_count != 0; --loose_count)
        {
            on_visit(slot_index, rank_index);
            slot_index = meta[slot_index].child_index[1];
            ++rank_index;
        }
    }
    if (visit_empty)
    {
        std::int32_t rank_index = static_cast<std::int32_t>(m_lexed_count + m_loose_count);
        std::int32_t slot_index = m_empty_list_head;
        for (std::uint32_t empty_count = m_empty_count; empty_count != 0; --empty_count)
        {
            on_visit(slot_index, rank_index);
            slot_index = meta[slot_index].child_index[1];
            ++rank_index;
        }
    }
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::private_resize(const std::uint32_t requested_capacity) noexcept
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
                    m_empty_count = m_capacity - m_lexed_count - m_loose_count;
                    m_empty_list_head = (m_empty_count != 0) ? state_to_list(0, static_cast<std::int32_t>(m_capacity - 1u), SlotState::is_empty_slot) : -1;
                }
                resized = true;
            }
        }
    }
    return resized;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::private_acquire(const std::int32_t slot_index, const bool lex, const bool require_unique, const bool allow_reserve) noexcept
{
    std::int32_t acquired_index = -1;
    if ((static_cast<std::uint32_t>(slot_index) + 1u) <= k_capacity_limit)
    {
        if (!require_unique || !safe_has_duplicate_key(slot_index))
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
                if (!lex)
                {
                    move_to_loose_list(acquired_index);
                }
                else if (lock(LockState::on_compare_keys))
                {
                    move_to_lexed_tree(acquired_index, slot_index);
                    unlock(LockState::on_compare_keys);
                }
                else
                {
                    acquired_index = -1;
                }
                const std::uint32_t occupied_count = m_lexed_count + m_loose_count;
                if (m_peak_usage < occupied_count)
                {
                    m_peak_usage = occupied_count;
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
    }
    return acquired_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::private_prev_lexed(const std::int32_t slot_index) const noexcept
{
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t prev_index = -1;
    std::int32_t from_index = meta[slot_index].child_index[0];
    if (from_index < 0)
    {
        for (from_index = slot_index; (((prev_index = meta[from_index].parent_index) >= 0) && (meta[prev_index].child_index[1] != from_index)); from_index = prev_index) {}
    }
    else
    {
        for (prev_index = from_index; ((from_index = meta[prev_index].child_index[1]) >= 0); prev_index = from_index) {}
    }
    return prev_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::private_next_lexed(const std::int32_t slot_index) const noexcept
{
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t next_index = -1;
    std::int32_t from_index = meta[slot_index].child_index[1];
    if (from_index < 0)
    {
        for (from_index = slot_index; (((next_index = meta[from_index].parent_index) >= 0) && (meta[next_index].child_index[0] != from_index)); from_index = next_index) {}
    }
    else
    {
        for (next_index = from_index; ((from_index = meta[next_index].child_index[0]) >= 0); next_index = from_index) {}
    }
    return next_index;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::private_has_duplicate_key(const std::int32_t slot_index) const noexcept
{
    return private_has_duplicate_key_in_lexed(slot_index) || private_has_duplicate_key_in_loose(slot_index);
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::private_has_duplicate_key_in_lexed(const std::int32_t slot_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    bool has_duplicate = false;
    std::int32_t lexed_index = -1;
    std::int32_t check_index = m_lexed_tree_root;
    while (check_index >= 0)
    {   //  find the first instance by lex of a matching slot
        std::int32_t relationship = on_compare_keys(slot_index, check_index);
        if (relationship == 0)
        {
            lexed_index = check_index;
        }
        check_index = meta[check_index].child_index[(relationship <= 0) ? 0 : 1];
    }
    if (lexed_index >= 0)
    {
        has_duplicate = true;
        if (lexed_index == slot_index)
        {   //  the found index needs to be excluded, find the next in-order index and see if it also matches
            check_index = meta[lexed_index].child_index[1];
            if (check_index < 0)
            {
                for (check_index = lexed_index; (((lexed_index = meta[check_index].parent_index) >= 0) && (meta[lexed_index].child_index[0] != check_index)); check_index = lexed_index) {}
            }
            else
            {
                for (lexed_index = check_index; ((check_index = meta[lexed_index].child_index[0]) >= 0); lexed_index = check_index) {}
            }
            if ((lexed_index < 0) || (on_compare_keys(slot_index, lexed_index) != 0))
            {   //  there is no non-excluded match
                has_duplicate = false;
            }
        }
    }
    return has_duplicate;
}

template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::private_has_duplicate_key_in_loose(const std::int32_t slot_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    bool has_duplicate = false;
    std::int32_t loose_index = m_loose_list_head;
    for (std::uint32_t loose_count = m_loose_count; loose_count != 0; --loose_count)
    {
        if (loose_index != slot_index)
        {
            std::int32_t relationship = on_compare_keys(slot_index, loose_index);
            if (relationship == 0)
            {
                has_duplicate = true;
                break;
            }
        }
        loose_index = meta[loose_index].child_index[1];
    }
    return has_duplicate;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::private_sort_and_compact(const bool use_external_payload) noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_move_payload);
    if (m_capacity != 0u)
    {
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t list_head = combine_lists(lexed_to_list(), combine_lists(m_loose_list_head, m_empty_list_head));
        if (use_external_payload)
        {
            std::int32_t slot_index = list_head;
            std::int32_t rank_index = 0;
            for (std::uint32_t count = m_capacity; count != 0; --count)
            {
                on_move_payload(slot_index, rank_index);
                slot_index = meta[slot_index].child_index[1];
                ++rank_index;
            }
        }
        else
        {
            std::uint32_t remaining = m_capacity;
            set_list_ordinals(list_head, m_capacity, 0);
            while (remaining)
            {
                std::int32_t cycle_start = list_head;

                std::int32_t slot_index = -1;
                std::int32_t rank_index = cycle_start;

                do
                {
                    Slot& rank_slot = meta[rank_index];

                    list_head = rank_slot.child_index[1];

                    meta[rank_slot.child_index[0]].child_index[1] = rank_slot.child_index[1];
                    meta[rank_slot.child_index[1]].child_index[0] = rank_slot.child_index[0];
                    --remaining;

                    rank_slot.child_index[1] = static_cast<TIndex>(slot_index);

                    slot_index = rank_index;
                    rank_index = rank_slot.parent_index;

                } while (rank_index != cycle_start);

                if (meta[slot_index].child_index[1] >= 0)
                {   //  process a multi-slot cycle

                    on_move_payload(slot_index, -1);

                    while ((rank_index = slot_index) >= 0)
                    {
                        slot_index = meta[rank_index].child_index[1];
                        on_move_payload(slot_index, rank_index);
                    }
                }
            }
        }

        const std::int32_t lexed_lower_index = 0;
        const std::int32_t lexed_upper_index = lexed_lower_index + static_cast<std::int32_t>(m_lexed_count - 1);

        const std::int32_t loose_lower_index = lexed_lower_index + static_cast<std::int32_t>(m_lexed_count);
        const std::int32_t loose_upper_index = loose_lower_index + static_cast<std::int32_t>(m_loose_count - 1);

        const std::int32_t empty_lower_index = loose_lower_index + static_cast<std::int32_t>(m_loose_count);
        const std::int32_t empty_upper_index = empty_lower_index + static_cast<std::int32_t>(m_empty_count - 1);

        m_lexed_tree_root = build_balanced_subtree(lexed_lower_index, lexed_upper_index, -1);
        m_loose_list_head = range_to_list(loose_lower_index, loose_upper_index, SlotState::is_loose_slot);
        m_empty_list_head = range_to_list(empty_lower_index, empty_upper_index, SlotState::is_empty_slot);

        m_high_index = static_cast<std::int32_t>(m_lexed_count + m_loose_count - 1u);
    }
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::build_balanced_subtree(const std::int32_t lower_index, const std::int32_t upper_index, const std::int32_t parent_index) noexcept
{
    std::int32_t split_index = -1;
    if (lower_index <= upper_index)
    {
        split_index = static_cast<std::int32_t>((static_cast<std::uint32_t>(lower_index) + static_cast<std::uint32_t>(upper_index)) >> 1);
        std::int32_t balance_factor = 0;
        for (std::int32_t children = upper_index - split_index; children; children >>= 1) ++balance_factor;
        for (std::int32_t children = split_index - lower_index; children; children >>= 1) --balance_factor;

        Slot& slot = m_meta_slot_array.data()[split_index];
        slot.parent_index = static_cast<TIndex>(parent_index);
        slot.child_index[0] = static_cast<TIndex>(build_balanced_subtree(lower_index, (split_index - 1), split_index));
        slot.child_index[1] = static_cast<TIndex>(build_balanced_subtree((split_index + 1), upper_index, split_index));
        slot.balance_factor = static_cast<TMeta>(balance_factor);
        slot.set_is_lexed_slot();
    }
    return split_index;
}

//  Convert the lexed tree structure into a ordered circular bi-directional list.
//  using Slot::child_index[] as prev/next links. Returns the list head slot index.
//
//  Note: This does not move payload items.
template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::lexed_to_list() noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    std::int32_t list_index = -1;
    std::int32_t scan_index = -1;
    for (list_index = m_lexed_tree_root; list_index >= 0; list_index = meta[scan_index = list_index].child_index[1]) {}
    if (scan_index >= 0)
    {
        std::int32_t from_index = -1;
        while (scan_index >= 0)
        {
            Slot& slot = meta[scan_index];
            slot.child_index[1] = static_cast<TIndex>(list_index);
            list_index = scan_index;
            from_index = slot.child_index[0];
            if (from_index < 0)
            {
                for (from_index = scan_index; ((scan_index = meta[from_index].parent_index) >= 0) && (meta[scan_index].child_index[1] != from_index); from_index = scan_index) {}
            }
            else
            {
                for (scan_index = from_index; (from_index = meta[scan_index].child_index[1]) >= 0; scan_index = from_index) {}
            }
        }
        from_index = -1;
        scan_index = list_index;
        while (scan_index >= 0)
        {
            Slot& slot = meta[scan_index];
            slot.parent_index = -1;
            slot.child_index[0] = static_cast<TIndex>(from_index);
            slot.balance_factor = 0;
            from_index = scan_index;
            scan_index = slot.child_index[1];
        }
        meta[list_index].child_index[0] = static_cast<TIndex>(from_index);
        meta[from_index].child_index[1] = static_cast<TIndex>(list_index);
    }
    return list_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::state_to_list(const std::int32_t lower_index, const std::int32_t upper_index, const SlotState state) noexcept
{
    std::int32_t head_index = -1;
    if ((lower_index <= upper_index) && (lower_index >= 0))
    {
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t prev_index = -1;
        std::int32_t next_index = -1;
        for (std::int32_t scan_index = upper_index; scan_index >= lower_index; --scan_index)
        {   //  scan backwards creating a singly linked forward list
            Slot& slot = meta[scan_index];
            if (slot.get_slot_state() == state)
            {
                slot.child_index[1] = next_index;
                next_index = scan_index;
            }
        }
        head_index = next_index;
        if (head_index != -1)
        {
            while (next_index != -1)
            {   //  scan the singly linked list patching it up to a bi-directional list
                Slot& slot = meta[next_index];
                slot.child_index[0] = prev_index;
                prev_index = next_index;
                next_index = slot.child_index[1];
            }

            //  fix up the list to be circular
            meta[head_index].child_index[0] = prev_index;
            meta[prev_index].child_index[1] = head_index;
        }
    }
    return head_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::range_to_list(const std::int32_t lower_index, const std::int32_t upper_index, SlotState state) noexcept
{
    if ((lower_index <= upper_index) && (lower_index >= 0))
    {
        Slot* const meta = m_meta_slot_array.data();
        for (std::int32_t scan_index = lower_index; scan_index <= upper_index; ++scan_index)
        {   //  scan the range creating new list members
            Slot& slot = meta[scan_index];
            slot.parent_index = -1;
            slot.child_index[0] = static_cast<TIndex>(scan_index - 1);
            slot.child_index[1] = static_cast<TIndex>(scan_index + 1);
            slot.balance_factor = 0;
            slot.set_slot_state(state);
        }

        //  fix up the list to be circular
        meta[upper_index].child_index[1] = static_cast<TIndex>(lower_index);
        meta[lower_index].child_index[0] = static_cast<TIndex>(upper_index);
        return lower_index;
    }
    return -1;
}

//  Convert a range of slot indices to a bi-directional list (slot metadata only).
template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::combine_lists(const std::int32_t list1_head_index, const std::int32_t list2_head_index) noexcept
{
    if (list1_head_index < 0)
    {
        return list2_head_index;
    }
    if (list2_head_index >= 0)
    {
        Slot* const meta = m_meta_slot_array.data();
        std::int32_t list1_tail_index = meta[list1_head_index].child_index[0];
        std::int32_t list2_tail_index = meta[list2_head_index].child_index[0];
        meta[list1_head_index].child_index[0] = static_cast<TIndex>(list2_tail_index);
        meta[list1_tail_index].child_index[1] = static_cast<TIndex>(list2_head_index);
        meta[list2_head_index].child_index[0] = static_cast<TIndex>(list1_tail_index);
        meta[list2_tail_index].child_index[1] = static_cast<TIndex>(list1_head_index);
    }
    return list1_head_index;
}

//  Set the parent_index of slots in a list to be an ordinal index
template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::set_list_ordinals(const std::int32_t list_index, const std::uint32_t list_count, const std::int32_t ordinal_start) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    std::int32_t ordinal_index = ordinal_start;
    std::int32_t slot_index = list_index;
    for (std::uint32_t slot_count = list_count; slot_count > 0; --slot_count)
    {
        Slot& slot = meta[slot_index];
        slot.parent_index = ordinal_index;
        slot_index = slot.child_index[1];
        ++ordinal_index;
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::append_range_to_loose_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept
{
    m_loose_count += static_cast<std::uint32_t>(upper_index - lower_index) + 1u;
    m_loose_list_head = combine_lists(m_loose_list_head, range_to_list(lower_index, upper_index, SlotState::is_loose_slot));
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::append_range_to_empty_list(const std::int32_t lower_index, const std::int32_t upper_index) noexcept
{
    m_empty_count += static_cast<std::uint32_t>(upper_index - lower_index) + 1u;
    m_empty_list_head = combine_lists(m_empty_list_head, range_to_list(lower_index, upper_index, SlotState::is_empty_slot));
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::attach_to_lexed(const std::int32_t slot_index, const std::int32_t key_index) noexcept
{
    Slot& slot = m_meta_slot_array.data()[slot_index];
    slot.parent_index = -1;
    slot.balance_factor = 0;
    slot.child_index[0] = slot.child_index[1] = -1;
    slot.set_is_lexed_slot();
    avl_insert(slot_index, key_index);
    ++m_lexed_count;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::attach_to_lexed(const std::int32_t slot_index) noexcept
{
    attach_to_lexed(slot_index, slot_index);
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::attach_to_loose(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    slot.parent_index = -1;
    slot.balance_factor = 0;
    slot.set_is_loose_slot();
    if (m_loose_list_head == -1)
    {
        slot.child_index[0] = slot.child_index[1] = slot_index;
    }
    else
    {
        slot.child_index[1] = m_loose_list_head;
        slot.child_index[0] = meta[m_loose_list_head].child_index[0];
        meta[slot.child_index[0]].child_index[1] = slot_index;
        meta[slot.child_index[1]].child_index[0] = slot_index;
    }
    m_loose_list_head = slot_index;
    ++m_loose_count;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::attach_to_empty(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    slot.parent_index = -1;
    slot.balance_factor = 0;
    slot.set_is_empty_slot();
    if (m_empty_list_head == -1)
    {
        slot.child_index[0] = slot.child_index[1] = slot_index;
    }
    else
    {
        slot.child_index[1] = m_empty_list_head;
        slot.child_index[0] = meta[m_empty_list_head].child_index[0];
        meta[slot.child_index[0]].child_index[1] = slot_index;
        meta[slot.child_index[1]].child_index[0] = slot_index;
    }
    m_empty_list_head = slot_index;
    ++m_empty_count;
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::remove_from_lexed(const std::int32_t slot_index) noexcept
{
    Slot& slot = m_meta_slot_array.data()[slot_index];
    --m_lexed_count;
    avl_remove(slot_index);
    slot.parent_index = -1;
    slot.child_index[0] = slot.child_index[1] = -1;
    slot.balance_factor = 0;
    slot.set_is_unassigned();
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::remove_from_loose(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    --m_loose_count;
    if (m_loose_list_head == slot_index)
    {
        m_loose_list_head = (m_loose_count == 0) ? -1 : slot.child_index[1];
    }
    if (m_loose_count != 0)
    {
        meta[slot.child_index[0]].child_index[1] = slot.child_index[1];
        meta[slot.child_index[1]].child_index[0] = slot.child_index[0];
    }
    slot.parent_index = -1;
    slot.child_index[0] = slot.child_index[1] = -1;
    slot.balance_factor = 0;
    slot.set_is_unassigned();
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::remove_from_empty(const std::int32_t slot_index) noexcept
{
    Slot* const meta = m_meta_slot_array.data();
    Slot& slot = meta[slot_index];
    --m_empty_count;
    if (m_empty_list_head == slot_index)
    {
        m_empty_list_head = (m_empty_count == 0) ? -1 : slot.child_index[1];
    }
    if (m_empty_count != 0)
    {
        meta[slot.child_index[0]].child_index[1] = slot.child_index[1];
        meta[slot.child_index[1]].child_index[0] = slot.child_index[0];
    }
    slot.parent_index = -1;
    slot.child_index[0] = slot.child_index[1] = -1;
    slot.balance_factor = 0;
    slot.set_is_unassigned();
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::move_to_lexed_tree(const std::int32_t slot_index, const std::int32_t key_index) noexcept
{
    switch (m_meta_slot_array.data()[slot_index].get_slot_state())
    {
        case (SlotState::is_loose_slot):
        {
            remove_from_loose(slot_index);
            attach_to_lexed(slot_index, key_index);
            break;
        }
        case (SlotState::is_empty_slot):
        {
            remove_from_empty(slot_index);
            attach_to_lexed(slot_index, key_index);
            break;
        }
        default:
        {
            break;
        }
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::move_to_lexed_tree(const std::int32_t slot_index) noexcept
{
    move_to_lexed_tree(slot_index, slot_index);
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::move_to_loose_list(const std::int32_t slot_index) noexcept
{
    switch (m_meta_slot_array.data()[slot_index].get_slot_state())
    {
        case (SlotState::is_lexed_slot):
        {
            remove_from_lexed(slot_index);
            attach_to_loose(slot_index);
            break;
        }
        case (SlotState::is_empty_slot):
        {
            remove_from_empty(slot_index);
            attach_to_loose(slot_index);
            break;
        }
        default:
        {
            break;
        }
    }
}

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::move_to_empty_list(const std::int32_t slot_index) noexcept
{
    switch (m_meta_slot_array.data()[slot_index].get_slot_state())
    {
        case (SlotState::is_lexed_slot):
        {
            remove_from_lexed(slot_index);
            attach_to_empty(slot_index);
            break;
        }
        case (SlotState::is_loose_slot):
        {
            remove_from_loose(slot_index);
            attach_to_empty(slot_index);
            break;
        }
        default:
        {
            break;
        }
    }
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::convert_to_rank_index(const std::int32_t slot_index) const noexcept
{
    std::int32_t rank_index = -1;
    const Slot* const meta = m_meta_slot_array.data();
    const SlotState state = meta[slot_index].get_slot_state();
    if (state == SlotState::is_lexed_slot)
    {
        std::int32_t scan_index = slot_index;
        while (scan_index >= 0)
        {
            ++rank_index;
            std::int32_t from_index = meta[scan_index].child_index[0];
            if (from_index < 0)
            {
                for (from_index = scan_index; (((scan_index = meta[from_index].parent_index) >= 0) && (meta[scan_index].child_index[1] != from_index)); from_index = scan_index) {}
            }
            else
            {
                for (scan_index = from_index; ((from_index = meta[scan_index].child_index[1]) >= 0); scan_index = from_index) {}
            }
        }
    }
    else if (state == SlotState::is_loose_slot)
    {
        rank_index = static_cast<std::int32_t>(m_lexed_count);
        for (std::int32_t scan_index = m_loose_list_head; scan_index != slot_index; scan_index = meta[scan_index].child_index[1])
        {
            ++rank_index;
        }
    }
    else if (state == SlotState::is_empty_slot)
    {
        rank_index = static_cast<std::int32_t>(m_lexed_count + m_loose_count);
        for (std::int32_t scan_index = m_empty_list_head; scan_index != slot_index; scan_index = meta[scan_index].child_index[1])
        {
            ++rank_index;
        }
    }
    return rank_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_by_rank_index(const std::int32_t rank_index) const noexcept
{
    std::int32_t slot_index = -1;
    if (rank_index >= 0)
    {
        const Slot* const meta = m_meta_slot_array.data();
        std::uint32_t search_count = static_cast<std::uint32_t>(rank_index);
        if (search_count < m_lexed_count)
        {
            std::uint32_t prev_side = 0;
            if ((search_count >> 1) > (m_lexed_count >> 1))
            {   //  search backwards from end
                prev_side = 1;
                search_count = m_lexed_count - search_count - 1u;
            }
            std::uint32_t next_side = prev_side ^ 1u;
            for (std::int32_t scan_index = m_lexed_tree_root; scan_index >= 0; scan_index = meta[slot_index = scan_index].child_index[prev_side]) {}
            while (search_count)
            {
                std::int32_t from_index = meta[slot_index].child_index[next_side];
                if (from_index < 0)
                {
                    for (from_index = slot_index; (((slot_index = meta[from_index].parent_index) >= 0) && (meta[slot_index].child_index[prev_side] != from_index)); from_index = slot_index) {}
                }
                else
                {
                    for (slot_index = from_index; ((from_index = meta[slot_index].child_index[prev_side]) >= 0); slot_index = from_index) {}
                }
                --search_count;
            }
        }
        else if ((search_count -= m_lexed_count) < m_loose_count)
        {
            slot_index = m_loose_list_head;
            std::uint32_t side = 1u;
            if (search_count > (m_loose_count >> 1))
            {
                side = 0u;
                search_count = (m_loose_count - search_count);
            }
            while (search_count != 0)
            {
                slot_index = meta[slot_index].child_index[side];
                --search_count;
            }
        }
        else if ((search_count -= m_loose_count) < m_empty_count)
        {
            slot_index = m_empty_list_head;
            std::uint32_t side = 1u;
            if (search_count > (m_empty_count >> 1))
            {
                side = 0u;
                search_count = (m_empty_count - search_count);
            }
            while (search_count != 0)
            {
                slot_index = meta[slot_index].child_index[side];
                --search_count;
            }
        }
    }
    return slot_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_any_equal(const std::int32_t key_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t found_index = m_lexed_tree_root;
    while (found_index >= 0)
    {
        std::int32_t relationship = on_compare_keys(key_index, found_index);
        if (relationship == 0)
        {
            break;
        }
        found_index = meta[found_index].child_index[(relationship < 0) ? 0 : 1];
    }
    return found_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_first_equal(const std::int32_t key_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t found_index = -1;
    std::int32_t check_index = m_lexed_tree_root;
    while (check_index >= 0)
    {
        std::int32_t relationship = on_compare_keys(key_index, check_index);
        if (relationship == 0)
        {
            found_index = check_index;
        }
        check_index = meta[check_index].child_index[(relationship <= 0) ? 0 : 1];
    }
    return found_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_first_greater(const std::int32_t key_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t found_index = -1;
    std::int32_t check_index = m_lexed_tree_root;
    while (check_index >= 0)
    {
        std::int32_t relationship = on_compare_keys(key_index, check_index);
        if (relationship < 0)
        {
            found_index = check_index;
        }
        check_index = meta[check_index].child_index[(relationship < 0) ? 0 : 1];
    }
    return found_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_first_greater_equal(const std::int32_t key_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t found_index = -1;
    std::int32_t check_index = m_lexed_tree_root;
    while (check_index >= 0)
    {
        std::int32_t relationship = on_compare_keys(key_index, check_index);
        if (relationship <= 0)
        {
            found_index = check_index;
        }
        check_index = meta[check_index].child_index[(relationship <= 0) ? 0 : 1];
    }
    return found_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_last_equal(const std::int32_t key_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t found_index = -1;
    std::int32_t check_index = m_lexed_tree_root;
    while (check_index >= 0)
    {
        std::int32_t relationship = on_compare_keys(key_index, check_index);
        if (relationship == 0)
        {
            found_index = check_index;
        }
        check_index = meta[check_index].child_index[(relationship >= 0) ? 1 : 0];
    }
    return found_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_last_less(const std::int32_t key_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t found_index = -1;
    std::int32_t check_index = m_lexed_tree_root;
    while (check_index >= 0)
    {
        std::int32_t relationship = on_compare_keys(key_index, check_index);
        if (relationship > 0)
        {
            found_index = check_index;
        }
        check_index = meta[check_index].child_index[(relationship > 0) ? 1 : 0];
    }
    return found_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::locate_last_less_equal(const std::int32_t key_index) const noexcept
{
    MV_HARD_ASSERT(m_lock == LockState::on_compare_keys);
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t found_index = -1;
    std::int32_t check_index = m_lexed_tree_root;
    while (check_index >= 0)
    {
        std::int32_t relationship = on_compare_keys(key_index, check_index);
        if (relationship >= 0)
        {
            found_index = check_index;
        }
        check_index = meta[check_index].child_index[(relationship >= 0) ? 1 : 0];
    }
    return found_index;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::min_occupied_index() const noexcept
{
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t slot_index = 0;
    for (std::uint32_t slot_count = m_capacity; slot_count > 0; --slot_count)
    {
        const Slot& slot = meta[slot_index];
        if (!slot.is_empty_slot())
        {
            return static_cast<std::int32_t>(slot_index);
        }
        ++slot_index;
    }
    return -1;
}

template<typename TIndex, typename TMeta>
inline std::int32_t TOrderedSlots<TIndex, TMeta>::max_occupied_index() const noexcept
{
    const Slot* const meta = m_meta_slot_array.data();
    std::int32_t slot_index = static_cast<std::int32_t>(m_capacity - 1u);
    for (std::uint32_t slot_count = m_capacity; slot_count > 0; --slot_count)
    {
        const Slot& slot = meta[slot_index];
        if (!slot.is_empty_slot())
        {
            return static_cast<std::int32_t>(slot_index);
        }
        --slot_index;
    }
    return -1;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::subtree_height(const std::int32_t slot_index) const noexcept
{
    if (slot_index >= 0)
    {
        Slot& slot = m_meta_slot_array.data()[slot_index];
        return std::max(subtree_height(slot.child_index[0]), subtree_height(slot.child_index[1])) + 1u;
    }
    return 0;
}

template<typename TIndex, typename TMeta>
inline std::uint32_t TOrderedSlots<TIndex, TMeta>::subtree_weight(const std::int32_t slot_index) const noexcept
{
    if (slot_index >= 0)
    {
        Slot& slot = m_meta_slot_array.data()[slot_index];
        return subtree_weight(slot.child_index[0]) + subtree_weight(slot.child_index[1]) + 1;
    }
    return 0;
}

//  This function should only be called on construction or after a call to shutdown().
template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::move_from(TOrderedSlots& src) noexcept
{
    m_capacity = src.m_capacity;
    m_peak_usage = src.m_peak_usage;
    m_peak_index = src.m_peak_index;
    m_high_index = src.m_high_index;
    m_lexed_count = src.m_lexed_count;
    m_loose_count = src.m_loose_count;
    m_empty_count = src.m_empty_count;
    m_lexed_tree_root = src.m_lexed_tree_root;
    m_loose_list_head = src.m_loose_list_head;
    m_empty_list_head = src.m_empty_list_head;
    m_meta_slot_array = std::move(src.m_meta_slot_array);
    m_lock = LockState::none;
    src.set_empty();
    return true;
}

//  This function should only be called on construction or after a call to shutdown().
template<typename TIndex, typename TMeta>
inline bool TOrderedSlots<TIndex, TMeta>::copy_from(const TOrderedSlots& src) noexcept
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
        m_lexed_count = src.m_lexed_count;
        m_loose_count = src.m_loose_count;
        m_empty_count = src.m_empty_count;
        m_lexed_tree_root = src.m_lexed_tree_root;
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

template<typename TIndex, typename TMeta>
inline void TOrderedSlots<TIndex, TMeta>::set_empty() noexcept
{
    m_capacity = 0;
    m_peak_usage = 0;
    m_peak_index = -1;
    m_high_index = -1;
    m_lexed_count = 0;
    m_loose_count = 0;
    m_empty_count = 0;
    m_lexed_tree_root = -1;
    m_loose_list_head = -1;
    m_empty_list_head = -1;
    m_meta_slot_array.deallocate();
    m_lock = LockState::none;
}

using COrderedSlots_int16 = TOrderedSlots<std::int16_t, std::int8_t>;
using COrderedSlots_int32 = TOrderedSlots<std::int32_t, std::int16_t>;

}   //  namespace slots

#endif  //  TORDERED_SLOTS_HPP_INCLUDED
