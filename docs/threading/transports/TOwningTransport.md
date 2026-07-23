Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TOwningTransport.md  
Author: Ritchie Brannan  
Date:   21 Apr 2026  

# TOwning

## Overview

`TOwning<T>` is a single-producer, single-consumer transport for
ownership-bearing typed elements.

It provides a fixed-capacity ring-backed transport with simple
rejection on overflow or underflow. The producer move-assigns into the
next writable live slot, the consumer move-assigns out of the next
readable live slot, and a single atomic occupied-count ledger
communicates the current number of readable elements.

`TOwning<T>` is intended to be a compact bounded owning transport
primitive:

- fixed-capacity only
- no growth
- no discard policy
- no overwrite-on-full behaviour
- single-element move-based post and read operations

`TOwning<T>` owns allocator-managed ring storage. Its memory attribution is a
transport-level configuration choice rather than an endpoint concern:

- `memory_context()` exposes the configured attribution as a read-only query
- attribution may be selected explicitly by constructor or `initialise(...)`
- `nullptr` context selection uses the ambient memory context
- attribution is fixed for the configured lifetime and is not reattributable
- endpoints do not expose or mutate attribution

It is not `TRing<T>` with a different payload category. The slot model
is materially different.

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.
- `T` must be non-copy constructible.
- `T` must be non-copy assignable.
- `T` must be nothrow default constructible.
- `T` must be nothrow destructible.
- `T` must be nothrow move constructible.
- `T` must be nothrow move assignable.

`TOwning<T>` provides:

- SPSC transport of ownership-bearing `T`
- single-element `post(T&&)`
- single-element `read(T&)`
- fixed-capacity bounded operation
- rejection when no writable slot exists
- rejection when no readable element exists
- role-specific and common status and validity checks
- read-only transport memory-context observation

`TOwning<T>` does not provide:

- growth
- discard
- overwrite-on-full behaviour
- blocking or waiting semantics
- multi-producer or multi-consumer use
- general-purpose shared random access
- bulk packet-style POD transfer semantics

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

The ring capacity is conditioned during initialisation to an internal
power-of-two capacity with a minimum floor.

## Slot lifetime model

Backing storage is raw storage until `initialise()`.

During `initialise()`:

- the transport's configured memory context is fixed for that configured lifetime
- storage is allocated
- every slot is default-constructed

After successful initialisation:

- all slots are live `T` objects
- slot lifetime persists until `deallocate()`
- occupancy controls logical readability and writability only

During operation:

- `post()` move-assigns into the next writable live slot
- `read()` move-assigns out of the next readable live slot
- moved-from slot objects remain live
- moved-from slot objects are later reused by move-assignment

During teardown:

- `deallocate()` destroys all slots
- destruction includes unread occupied elements still resident in the
  transport

## Ownership model

Ownership transfer boundaries are operation-based, not lifetime-based.

On successful `post(T&&)`:

- ownership transfers into the destination ring slot

On successful `read(T&)`:

- ownership transfers out of the source ring slot into `dst`

Unread elements are owned by the transport until they are read or
destroyed by `deallocate()`.

`deallocate()` is therefore destructive teardown of resident elements,
not just raw storage release.

## Observation model

### Shared observations

`is_valid()` and `is_ready()` are intentionally shallow.

They reflect stable structural state, not a deep concurrent proof of
every transient relationship between indices, occupancy, and slot
contents.

`writable_count()` and `readable_count()` are observational snapshots.
They do not reserve writable slots or readable elements against later
concurrent change.

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

## Capacity and indexing model

User-supplied initialisation capacity is treated as a minimum requested
capacity, not necessarily the exact internal capacity.

Initialisation rules:

- requests greater than `k_max_capacity` fail
- accepted requests are rounded up to a power of 2
- accepted requests are clamped to a minimum floor of
  `k_min_capacity`

Once initialised, capacity does not change.

Ring wrap uses power-of-two masking over the conditioned internal
capacity.

## Ledger model

The shared atomic `m_occupied_count` is the only cross-thread transport
ledger.

Producer behaviour:

- move-assigns payload into a writable live slot
- publishes newly occupied state by incrementing `m_occupied_count`

Consumer behaviour:

- move-assigns payload out of a readable live slot
- releases consumed state by decrementing `m_occupied_count`

This gives the transport a simple bounded SPSC model:

- producer owns write progression
- consumer owns read progression
- occupied count is the only shared quantity used to coordinate
  readable versus writable state

## Post semantics

`post(T&&)` is all-or-nothing.

A post succeeds only if at least one writable slot currently exists.

If no writable slot exists:

- the post fails
- no slot is written
- producer-local state is not advanced

For successful posting:

- `src` is move-assigned into the next writable live slot
- write index is advanced modulo capacity
- occupied count is incremented by 1

## Read semantics

`read(T&)` is all-or-nothing.

A read succeeds only if at least one readable element currently exists.

If no readable element exists:

- the read fails
- no slot is consumed
- consumer-local state is not advanced

For successful reading:

- the next readable live slot is move-assigned into `dst`
- read index is advanced modulo capacity
- occupied count is decremented by 1

## Status and validity

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

### `posting_is_valid()`

Producer-side shallow validity check.

Safe only:

- on the producer thread
- or while quiescent

Intended meaning:

- common structural state is valid
- producer-local write index is within range for the current readiness
  state

### `reading_is_valid()`

Consumer-side shallow validity check.

Safe only:

- on the consumer thread
- or while quiescent

Intended meaning:

- common structural state is valid
- consumer-local read index is within range for the current readiness
  state

## Setup and teardown

`initialise()` requires a deallocated instance.

Re-initialisation without `deallocate()` fails.

`deallocate()`:

- destroys all live slots
- releases owned ring storage
- restores canonical empty state

`deallocate()` requires quiescence and must not race producer or
consumer activity.

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
- all slots are live `T` objects
- read index is within ring bounds
- write index is within ring bounds
- occupied count is in the range `[0, capacity]`

While deallocated, the intended invariants are:

- ring storage is null
- capacity is zero
- read index is zero
- write index is zero
- occupied count is zero

## Differences from TRing

`TOwning<T>` differs materially from `TRing<T>`.

`TRing<T>`:

- stores POD payload bytes
- relies on trivially copyable transfer
- does not construct or destroy slot objects

`TOwning<T>`:

- stores live `T` objects in every slot after initialisation
- has explicit construction and destruction responsibilities
- uses move assignment for transfer
- destroys unread resident elements during teardown

## Non-goals and caveats

- `TOwning<T>` is not a growable queue
- `TOwning<T>` does not preserve writes by overwriting older unread
  data
- writable and readable counts are snapshot observations, not
  reservations
- common validity is intentionally shallow
- teardown is destructive for unread resident elements
