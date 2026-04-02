Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TOrderedSlots.md  
Author: Ritchie Brannan  
Date:   1 Apr 26  

# TOrderedSlots<TIndex, TMeta>

## Purpose

TOrderedSlots<TIndex, TMeta> is a general-purpose ordering and
slot-management toolkit that maintains an ordered index over selected
occupied slots.

It provides:

- Slot acquisition and recycling
- Optional ordering via an AVL tree
- Deterministic traversal via rank indexing
- Structural mutation and compaction utilities

It stores metadata only and does not store or access payload.

This template is intended as a base class, not a concrete container.

## Toolkit nature

TOrderedSlots is a composable structural layer.

The derived class:

- owns payload storage
- defines key comparison
- implements payload movement
- controls capacity approval
- exposes the public API

Ordering and traversal are optional and depend on derived usage.

## Interface model

The interface is primarily protected.

The base provides metadata and structure. The derived class defines
public behaviour.

## Structural model

Each slot is in exactly one state:

- lexed
- loose
- empty
- unassigned

Structures:

- lexed -> AVL tree
- loose -> circular doubly-linked list
- empty -> circular doubly-linked list

Stable-state invariant:

    lexed_count() + loose_count() + empty_count() == capacity()

## Terminology

"lexed" means ordered according to on_compare_keys().

It does not imply textual or string-based ordering.

## Slot index

Slot index domain:

    [0, capacity())

- identifies metadata and derived payload
- stable except during sort_and_pack()

Sentinel:

- -1 is not a valid slot index
- may be used as failure sentinel
- may be used as query operand in on_compare_keys()

## Rank model

Traversal order:

    lexed (ordered) -> loose -> empty

Rank index is defined by traversal order.

- lexed: [0, lexed_count())
- loose: [lexed_count(), lexed_count() + loose_count())
- empty: no rank

Traversal order defines rank.

After sort_and_pack():

- slot_index == rank_index for all slots

## Equal key ordering

Equal keys preserve insertion order.

Equal-key runs are stable across:

- insertion
- relex_all()
- sort_and_pack()

## Ownership boundary

Base owns:

- metadata
- structure
- lifecycle

Derived owns:

- payload
- key definition
- payload movement
- capacity policy

## Virtual callbacks

Derived provides:

- on_visit(slot_index, rank_index)
- on_move_payload(source_index, target_index)
- on_reserve_empty(minimum_capacity, recommended_capacity)
- on_compare_keys(source_index, target_index)

### on_compare_keys

Defines ordering.

source_index == -1 represents staged query key.

Comparator must be strict weak ordering.

### on_move_payload

Applies payload remap during sort_and_pack().

Supports:

- in-place remap using temporary storage (-1)
- external payload remap

### on_reserve_empty

Capacity negotiation.

Must satisfy minimum_capacity.

### on_visit

Called during traversal with slot_index and rank_index.

## Re-entry guard

Structural re-entry during callbacks is prohibited.

Safe accessor subset only.

- Debug: hard fail
- Release: soft fail

No thread safety.

## Internal layering

- protected interface
- safe_* wrappers
- private_* helpers

Mutation occurs under a single guard.

## Mutation model

Operations:

- lex_all()
- unlex_all()
- relex_all()
- sort_and_pack()

sort_and_pack():

- reorders payload and metadata
- produces packed canonical layout

## Capacity model

- capacity()
- minimum_safe_capacity()
- capacity_limit()

Operations:

- safe_resize()
- reserve_empty()
- reserve_and_acquire()
- shrink_to_fit()

No automatic shrink.

## Lifecycle

Destructive:

- initialise()
- shutdown()
- clear()

Non-destructive:

- resize
- reserve
- acquire
- erase

## Visit operations

Visit order:

    lexed -> loose -> empty

Rank ranges:

- lexed: ordered
- loose: appended
- empty: trailing

## Invariants

- exclusive slot categories
- counts sum to capacity
- AVL tree valid
- lists circular
- high_index correct

## Complexity

- AVL operations: O(log n)
- list operations: O(1)
- sort_and_pack(): O(n)

## Out of scope

- thread safety
- payload management
- comparator validation
- automatic packing
