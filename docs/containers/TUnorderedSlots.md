# TUnorderedSlots<TIndex>

## Purpose

TUnorderedSlots<TIndex> maintains an unordered index over slot
indices.

It stores metadata only (list links, slot state, and counts) and
does not store or access payload.

This template is intended as a base class, not a concrete
container. The derived class:

- Owns payload storage
- Implements payload movement
- Controls capacity approval
- Exposes the public API

Single-threaded.

---

## Interface Model

The interface is almost entirely protected.

- Protected functions form the derived-facing API.
- The derived class defines the public interface.
- The base acts as an internal metadata engine.

---

## Structural Model

Each slot is in exactly one state:

- loose      - occupied
- empty      - available for acquisition
- unassigned - internal transitional state
- terminator - internal sentinel state

Structures:

- Loose  -> circular doubly-linked list
- Empty  -> circular doubly-linked list

No overlap between categories.

Stable-state invariant:

    loose_count() + empty_count() == capacity()

---

## Slot Index

A slot index is a signed integer in:

    [0, capacity())

It addresses metadata in the base and payload in the derived
class.

Slot indices are stable except during pack().

### Sentinel Conventions

- -1 is never a valid slot index.
- -1 may be returned as a failure sentinel.
- Slot index and visit identifiers are separate domains.

Visit identifiers are used only for on_visit() dispatch:

- loose slots use identifier -1
- empty slots use identifier -2

---

## Ownership Boundary

Base owns:

- Slot metadata
- Structural bookkeeping
- Slot lifecycle and categorisation

Derived owns:

- Payload storage and lifetime
- Payload relocation
- Capacity policy
- Public API

The base never inspects payload.

---

## Virtual Callbacks

The derived class provides the following overrides:

- on_visit(slot_index, identifier)
- on_move_payload(source_index, target_index)
- on_reserve_empty(minimum_capacity, recommended_capacity)

### on_visit

Called during visit operations with slot_index and identifier.

Identifier values used by this template:

- (-1) for empty slots
- (-2) for loose slots

### on_move_payload

Moves payload between slot indices.

Contract:

- source_index != target_index
- Exactly one of source_index or target_index may be -1
- -1 denotes temporary storage owned by the derived class
- Both are never -1

Only called by pack().

### on_reserve_empty

Handshake for capacity growth.

The base supplies:

- minimum_capacity, which must be satisfied
- recommended_capacity, a growth heuristic

The derived class returns the capacity to apply after ensuring
payload storage is ready. Returning a value less than
minimum_capacity causes the caller to fail.

---

## Re-Entry Guard

Structural re-entry during virtual callbacks is prohibited.

Only a restricted set of protected accessors is safe inside
callbacks. All other protected functions are unsafe.

Enforcement:

- Debug builds hard fail.
- Release builds soft fail by returning false or -1 without
  mutation.

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

This structure permits batched mutation under a single guard and
avoids nested locking.

---

## Mutation Model

Structural mutation operations:

- pack()

During mutation:

- Structure may be temporarily inconsistent.
- Integrity checks are valid only after completion.
- Execution occurs under a single guard.

### pack()

Physically reorders payload and rebuilds metadata.

After completion:

- Loose payload occupies slot indices [0, loose_count()).
- Remaining slots are empty in [loose_count(), capacity()).

Reordering uses on_move_payload() and may use temporary -1 moves.
The specific move sequence is an internal implementation detail.
The derived class must support the on_move_payload() contract.

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
- reserve_empty() and reserve_and_acquire() do not invalidate occupied slots
- shrink_to_fit() is equivalent to safe_resize(high_index() + 1)

No automatic shrinking or compaction is performed.

---

## Lifecycle

Destructive:

- initialise(capacity)
- shutdown()
- clear()

clear() resets metadata and rebuilds the empty list without
deallocation.

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
class is responsible for handling or discarding payload.

---

## Visit Operations

Visits may include one or more categories.

Each visited slot calls:

    on_visit(slot_index, identifier)

Identifier values:

- loose: -1
- empty: -2

---

## Invariants (Stable State)

- Each slot is in exactly one category
- Category counts sum to capacity()
- Loose and empty lists are circular and bidirectional
- high_index() is the highest occupied index or -1
- peak_usage() and peak_index() are monotonic maxima

Integrity checks assume no active mutation.

---

## Complexity

- Acquire and erase: O(1)
- List operations: O(1)
- pack(): O(n)
- check_integrity(): O(n)

---

## Out of Scope

The template does not:

- Provide thread safety
- Manage payload memory
- Provide ordering or key comparison
- Validate on_move_payload correctness
- Auto-shrink without explicit calls
