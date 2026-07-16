Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TRingTransport.md  
Author: Ritchie Brannan  
Date:   15 Apr 2026  

# TRing

## Overview

`TRing<T>` is a single-producer, single-consumer transport for
trivially copyable typed elements.

It provides a fixed-capacity ring-backed transport with simple
rejection on overflow or underflow. The producer writes into the ring
at its local write index, the consumer reads from the ring at its local
read index, and a single atomic occupied-count ledger communicates the
current number of readable elements.

`TRing<T>` is intended to be a compact bounded transport primitive:

- fixed-capacity only
- no growth
- no discard policy
- no overwrite-on-full behaviour
- all-or-nothing post and read operations

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.
- `T` must be non-const.
- `T` must be trivially copyable.

`TRing<T>` provides:

- SPSC transport of trivially copyable `T`
- single-element and bulk `post()`
- single-element and bulk `read()`
- fixed-capacity bounded operation
- rejection when insufficient writable space exists
- rejection when insufficient readable elements exist
- role-specific and common status and validity checks

`TRing<T>` does not provide:

- producer-side growth
- producer-side discard
- overwrite-on-full behaviour
- blocking or waiting semantics
- multi-producer or multi-consumer use
- general-purpose shared random access

## State model

The transport state consists of:

- ring storage
- active ring capacity
- producer-owned write index
- consumer-owned read index
- shared occupied-count ledger

The producer owns:

- `m_write_index`

The consumer owns:

- `m_read_index`

Both sides observe the shared atomic:

- `m_occupied_count`

## Observation model

### Producer-side observations

The producer may safely reason about:

- local write index
- configured capacity
- occupied count loaded from the shared atomic
- writable count derived from capacity and occupied count

The producer does not own the read index.

### Consumer-side observations

The consumer may safely reason about:

- local read index
- configured capacity
- occupied count loaded from the shared atomic
- readable count derived from the occupied count

The consumer does not own the write index.

### Common observations

`is_valid()` and `is_ready()` are intentionally shallow.

They reflect stable structural state, not a deep concurrent proof of
every transient relationship between local indices and occupancy.

`writable_count()` and `readable_count()` are observational snapshots.
They do not reserve space or elements against later concurrent change.

## Capacity model

User-supplied initialisation capacity is treated as a minimum requested
capacity, not necessarily the exact internal capacity.

Initialisation rules:

- requests greater than `k_max_capacity` fail
- accepted requests are rounded up to a power of 2
- accepted requests are clamped to a minimum floor of `k_min_capacity`

Once initialised, capacity does not change.

## Ledger model

The shared atomic `m_occupied_count` is the only cross-thread transport
ledger.

Producer behaviour:

- writes payload data into ring storage
- publishes newly occupied elements by incrementing `m_occupied_count`

Consumer behaviour:

- reads payload data from ring storage
- releases consumed elements by decrementing `m_occupied_count`

This gives the transport a simple bounded SPSC model:

- producer owns write progression
- consumer owns read progression
- occupied count is the only shared quantity used to coordinate
  readable vs writable space

## Post semantics

`post()` is all-or-nothing.

A post succeeds only if the entire requested count fits in the
currently writable region of the transport.

If there is insufficient writable capacity:

- the post fails
- no elements are added
- producer-local state is not advanced

For successful posting:

- elements are copied into the ring in order
- wrap is handled internally if the write crosses the
  logical end of the ring
- write index is advanced modulo capacity
- occupied count is incremented by the posted count

Bulk posting does not partially succeed.

Pointer rules:

- `src == nullptr` with `count != 0` fails
- `src == nullptr` with `count == 0` succeeds if the transport is ready
- the view overload requires a valid view; a zero-length invalid view fails

## Read semantics

`read()` is all-or-nothing.

A read succeeds only if the entire requested count is currently
readable.

If there are insufficient readable elements:

- the read fails
- no elements are removed
- consumer-local state is not advanced

For successful reading:

- elements are copied out of the ring in order
- wrap is handled internally if the read crosses the
  logical end of the ring
- read index is advanced modulo capacity
- occupied count is decremented by the read count

Bulk reading does not partially succeed.

Pointer rules:

- `dst == nullptr` with `count != 0` fails
- `dst == nullptr` with `count == 0` succeeds if the transport is ready
- the view overload requires a valid view; a zero-length invalid view fails

## Status and validity

### `posting_is_valid()`

Producer-side validity check.

Safe only:

- on the producer thread
- or while quiescent

Intended meaning:

- common structural state is valid
- producer-local write index is within range for the current readiness state

### `reading_is_valid()`

Consumer-side validity check.

Safe only:

- on the consumer thread
- or while quiescent

Intended meaning:

- common structural state is valid
- consumer-local read index is within range for the current readiness state

### `is_valid()`

Common shallow structural validity check.

Intended meaning:

- canonical empty state is coherent, or
- initialised storage exists and occupied count lies within the range  
  `[0, capacity]`

This is not a deep concurrent invariant audit.

### `is_ready()`

Shallow operational readiness check.

Intended meaning:

- transport has been initialised
- backing storage exists

## Setup and teardown

`initialise()` requires a deallocated instance.

Re-initialisation without `deallocate()` fails.

`deallocate()` releases owned ring storage and restores canonical empty state.

Direct setup and teardown are intended for owner or registry control
rather than arbitrary role-side use.

## Canonical empty state

Canonical empty state means:

- no ring storage
- zero capacity
- zero read index
- zero write index
- zero occupied count

This is the deallocated state.

## Invariants

While ready, the intended invariants are:

- ring storage is allocated
- capacity is non-zero
- read index is within ring bounds
- write index is within ring bounds
- occupied count is in the range `[0, capacity]`

While deallocated, the intended invariants are:

- ring storage is null
- capacity is zero
- read index is zero
- write index is zero
- occupied count is zero

## Non-goals and caveats

- `TRing<T>` is not a growable queue
- `TRing<T>` does not preserve writes by overwriting older unread data
- `TRing<T>` does not provide retry or reservation semantics beyond
  explicit caller retry
- common validity is intentionally shallow
