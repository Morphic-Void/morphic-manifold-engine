
Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

# TOrderedSlots<TIndex, TMeta>

## Purpose

TOrderedSlots<TIndex, TMeta> is a general-purpose ordering and slot-management toolkit that maintains an ordered index over selected occupied slots.

It provides:

- Slot acquisition and recycling
- Optional ordering via an AVL tree
- Deterministic traversal via rank indexing
- Structural mutation and compaction utilities

It stores metadata only (tree links, list links, balance factor,
state, and counts) and does not store or access payload.

This template is intended as a base class, not a concrete
container. The derived class:

- Owns payload storage
- Defines key comparison
- Implements payload movement
- Controls capacity approval
- Exposes the public API

Single-threaded.

---

## Toolkit Nature

TOrderedSlots is not a fully realised container. It is a composable structural layer
that enables a range of container designs depending on which features are used.

In its minimal form:

- It can act purely as a slot recycler (similar in spirit to TUnorderedSlots)
- Ordering can be entirely unused if no comparator is provided
- Visiting is optional if traversal is not required

Higher-level behaviour emerges only when the derived class supplies:

- A comparator (enabling ordering)
- Payload movement (enabling compaction)
- Public APIs (defining semantics)

### Relationship to TUnorderedSlots

TUnorderedSlots is a reduced form of this model:

- No AVL tree
- No comparison logic
- Smaller metadata footprint
- Pure slot recycling

TOrderedSlots can be viewed as a superset that adds optional ordering and traversal
structure on top of the same fundamental slot lifecycle model.

---

## Interface Model

The interface is almost entirely protected.

- Protected functions form the derived-facing API.
- The derived class defines the public interface.
- The base acts as an internal metadata engine.

---

## Structural Model

Each slot is in exactly one state:

- lexed      - occupied and a member of the AVL tree
- loose      - occupied but not lexed
- empty      - available for acquisition
- unassigned - internal transitional state

Structures:

- Lexed  -> AVL tree
- Loose  -> circular doubly-linked list
- Empty  -> circular doubly-linked list

No overlap between categories.

Stable-state invariant:

    lexed_count() + loose_count() + empty_count() == capacity()

---

## Terminology: "Lexed"

The term "lexed" is historical and does not imply string or textual comparison.

- "Lexed" means ordered according to the comparator
- Ordering is entirely defined by on_compare_keys()
- The comparator may represent numeric, spatial, temporal, or arbitrary ordering

No assumptions are made about the nature of the key.

---

## Slot Index

A slot index is a signed integer in:

    [0, capacity())

It addresses metadata in the base and payload in the derived class.

Slot indices are stable except during sort_and_pack().

### Sentinel Conventions

- -1 is never a valid slot index
- -1 may be returned as a failure sentinel
- -1 may be passed to on_compare_keys() as the query key operand
- Slot index and rank index are separate domains

When -1 is passed as source_index to on_compare_keys(), it represents
the derived class's currently staged query or insert key.

---

## Lex Order and Rank Index

Lex order is defined by on_compare_keys().

Rank index is the zero-based traversal index under the canonical order:

    lexed (in comparator-defined order), then loose

Formally:

- Lexed slots have rank in [0, lexed_count())
- Loose slots have rank in [lexed_count(), lexed_count() + loose_count())
- Empty slots have no rank

Because lexed slots are traversed in comparator-defined order, rank for
lexed slots is also their ordering index.

Rank represents the physical index a slot will occupy after
sort_and_pack() and is intended to be computed prior to
compaction.

---

## Equal Key Ordering

Ordering is defined by the comparator provided via
on_compare_keys().

If on_compare_keys(a, b) returns 0, the keys are considered equal.

Among equal keys, ordering is stable by insertion order.

When inserting a key equal to existing keys, the new slot is
placed as the in-order successor of the last equal key. Equal
runs therefore grow at the end.

Bulk rebuild operations preserve ordering:

- relex_all() preserves the existing in-order sequence
- sort_and_pack() preserves the existing in-order sequence,
  including equal-key runs

---

## Ownership Boundary

Base owns:

- Slot metadata
- Structural bookkeeping
- Slot lifecycle and categorisation

Derived owns:

- Payload storage and lifetime
- Key definition and ordering
- Payload relocation
- Capacity policy
- Public API

The base never inspects payload and never interprets keys.

---

## Virtual Callbacks

The derived class provides the following overrides:

- on_visit(slot_index, rank_index)
- on_move_payload(source_index, target_index)
- on_reserve_empty(minimum_capacity, recommended_capacity)
- on_compare_keys(source_index, target_index)

### Optionality

Not all callbacks are required for all use cases:

- on_compare_keys is only required if ordering is used
- on_visit is only required if traversal APIs are used
- on_move_payload is only required if sort_and_pack() is used

The template does not enforce usage. The derived class must
only invoke operations that match its implementation.

### on_compare_keys

Defines the ordering used by the lexed AVL tree.

Returns:

- < 0 if key(source) < key(target)
- 0   if key(source) == key(target)
- > 0 if key(source) > key(target)

Query-key operand:

Some search and acquisition operations compare an external
query key against lexed slots by calling:

    on_compare_keys(-1, target_index)

The base does not store this query key. The derived class must
stage the query or insert key in its own storage before calling
search or acquire operations. That staged key must remain valid
for the duration of the call.

When source_index == -1, on_compare_keys() must compare the
staged key against key(target_index).

Comparator correctness is assumed. A comparator that is not a
strict weak ordering breaks structural correctness.

### on_move_payload

Moves payload between slot indices.

Contract:

- source_index != target_index
- Exactly one of source_index or target_index may be -1
- -1 denotes temporary storage owned by the derived class
- Both are never -1

Only called by sort_and_pack().

### on_reserve_empty

Handshake for capacity growth.

The base supplies:

- minimum_capacity, which must be satisfied
- recommended_capacity, a growth heuristic

The derived class returns the capacity to apply after ensuring
payload storage is ready. Returning a value less than
minimum_capacity causes the caller to fail.

### on_visit

Called during visit operations with slot_index and rank_index.

---

## Re-Entry Guard

Structural re-entry during virtual callbacks is prohibited.

Only a restricted set of protected accessors is safe inside
callbacks. All other protected functions are unsafe.

Enforcement:

- Debug builds hard fail
- Release builds soft fail by returning false or -1 without mutation

No thread safety is provided.

Integrity checks are valid only in stable state, not during
mutation or callback dispatch.

---

## Internal Layering Model

Three internal layers:

Protected interface:

- Derived-facing entry points
- Validate state
- Establish and release guard state

safe_* wrappers:

- Acquire and release the guard around virtual dispatch

private_* helpers:

- Core mutation and traversal logic
- Assume validated preconditions
- Do not acquire locks directly
- May invoke callbacks only via safe_* wrappers

This structure permits batched mutation under a single guard
and avoids nested locking.

---

## Mutation Model

Structural mutation operations:

- lex_all()
- unlex_all()
- relex_all()
- sort_and_pack()

During mutation:

- Structure may be temporarily inconsistent
- Integrity checks are valid only after completion
- Execution occurs under a single guard

### lex_all()

Moves all loose slots into the lexed tree.

- Payload unchanged
- Ordered via on_compare_keys()
- No uniqueness enforcement by default

If uniqueness is required, it must be ensured by
the derived class.

### unlex_all()

Moves all lexed slots to loose.

- Payload unchanged
- Ordering information is discarded

### relex_all()

Rebuilds ordering of currently lexed slots.

- Payload unchanged
- Loose and empty membership unchanged
- Existing in-order sequence is preserved

### sort_and_pack()

Physically reorders payload into canonical packed order and rebuilds metadata.

After completion:

- Lexed slots occupy indices [0, lexed_count()) in lex order
- Loose slots occupy [lexed_count(), lexed_count() + loose_count())
- Empty slots occupy [lexed_count() + loose_count(), capacity())

Reordering uses on_move_payload():

- In-place mode: uses cycle resolution (temporary -1 moves)
- External mode: performs a single pass to a complete external domain

On completion:

- Logical traversal order is unchanged
- Occupied region is traversal-contiguous
- For all slots: slot_index == rank_index

The rank mapping becomes identity:

- rank_to_slot[rank] == rank
- slot_to_rank[slot] == slot

---

## Capacity Model

Definitions:

- capacity()
- minimum_safe_capacity() == high_index() + 1
- index_limit()
- capacity_limit() == index_limit() + 1

minimum_safe_capacity() may be transiently inconsistent during
mutation but is corrected before stable state.

Operations:

- safe_resize(new_capacity)
- reserve_empty(slot_count)
- reserve_and_acquire(...)
- shrink_to_fit()

Non-destructive guarantees:

- safe_resize() fails if new_capacity < high_index() + 1
- reserve operations do not invalidate occupied slots
- shrink_to_fit() == safe_resize(high_index() + 1)

No automatic shrinking or compaction is performed.

---

## Lifecycle

Destructive:

- initialise(capacity)
- shutdown()
- clear()

clear() resets metadata and rebuilds the empty list without deallocation.

Non-destructive:

Capacity management:

- safe_resize(new_capacity)
- reserve_empty(slot_count)
- reserve_and_acquire(...)
- shrink_to_fit()

Slot operations:

- acquire(...)
- erase(slot_index)

erase() returns an occupied slot to the empty list. The derived
class is responsible for payload handling.

---

## Visit Operations

Visits may include one or more categories.

If multiple categories are included, dispatch order is:

    lexed -> loose -> empty

Each visited slot calls:

    on_visit(slot_index, rank_index)

Rank index during visit:

- Lexed: [0, lexed_count())
- Loose: [lexed_count(), lexed_count() + loose_count())
- Empty: [lexed_count() + loose_count(), capacity())

---

## Invariants (Stable State)

- Each slot is in exactly one category
- Category counts sum to capacity()
- AVL tree links are consistent and balanced
- Loose and empty lists are circular and bidirectional
- high_index() is the highest occupied index or -1
- peak_usage() and peak_index() are monotonic maxima

Integrity checks assume no active mutation.

---

## Complexity

- Lexed insert and remove: O(log n)
- Find operations: O(log n)
- Duplicate detection: O(log n) in lexed, O(n) in loose
- sort_and_pack(): O(n)
- List operations: O(1)

---

## Out of Scope

The template does not:

- Provide thread safety
- Manage payload memory
- Guarantee uniqueness unless enforced by the derived class
- Validate comparator correctness
- Auto-pack or auto-shrink
- Define ordering beyond on_compare_keys()

