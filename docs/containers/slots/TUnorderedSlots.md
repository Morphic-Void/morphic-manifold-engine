Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TUnorderedSlots.md  
Author: Ritchie Brannan  
Date:   1 Apr 26  

# TUnorderedSlots<TIndex>

## Overview

TUnorderedSlots<TIndex> maintains an unordered index over slot indices.

The template stores metadata only and does not store or access payload.

This template is intended as a base class.

## Scope

- metadata only
- no payload ownership
- no payload construction or destruction

## State model

Each slot is in one steady-state category:

- loose
- empty

Internal states:

- unassigned
- terminator

Structures:

- loose -> circular doubly-linked list
- empty -> circular doubly-linked list

Invariant:

    loose_count() + empty_count() == capacity()

## Slot index

Domain:

    [0, capacity())

Sentinel:

- -1 is not a valid slot index

Visit identifiers:

- -1 for loose
- -2 for empty

## Ownership boundary

Base owns:

- metadata
- structure
- lifecycle

Derived owns:

- payload
- movement
- capacity policy
- public API

## Observation model

Traversal:

- defined by list order
- does not imply rank

Rank:

- defined over loose slots only
- rank(slot_index) = number of loose slots with lower slot index
- domain: [0, loose_count())
- empty slots return -1

## Pack model

pack():

- compacts loose slots into [0, loose_count())
- remaining slots become empty
- rebuilds lists in ascending slot index order

Non-goals:

- no empty-slot preservation
- no full-domain remapping

## Virtual callbacks

Derived provides:

- on_visit(slot_index, identifier)
- on_move_payload(source_index, target_index)
- on_reserve_empty(minimum_capacity, recommended_capacity)

### on_move_payload

- source_index != target_index
- indices are non-negative
- source is loose
- target in [0, loose_count())

Empty-slot overwrite allowed.

### on_reserve_empty

Capacity negotiation.

Must satisfy minimum_capacity.

### on_visit

Called with identifier:

- -1 loose
- -2 empty

## Re-entry guard

No structural re-entry during callbacks.

- Debug: hard fail
- Release: soft fail

No thread safety.

## Internal layering

- protected interface
- safe_* wrappers
- private_* helpers

## Mutation model

Operation:

- pack()

## Capacity model

- capacity()
- minimum_safe_capacity()
- capacity_limit()

Operations:

- safe_resize()
- reserve_empty()
- reserve_and_acquire()
- shrink_to_fit()

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

Each visit calls:

    on_visit(slot_index, identifier)

## Invariants

- exclusive categories
- counts sum to capacity
- lists circular
- high_index valid

## Complexity

- O(1) list operations
- pack(): O(n)

## Alignment with TOrderedSlots

TOrderedSlots:

- full-domain rank
- total reordering

TUnorderedSlots:

- occupied-domain rank
- compaction only

## Out of scope

- thread safety
- payload management
- ordering
- comparator validation
