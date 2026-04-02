Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TStableStorage.md  
Author: Ritchie Brannan  
Date:   1 Apr 2026  

# TStableStorage<T>

## Overview

TStableStorage<T> provides stable segmented storage for typed data.

The implementation owns multiple fixed-size buffers of T. Buffers are
allocated on demand and never relocated. Slot indices map to buffer
positions via bit arithmetic.

Address stability is guaranteed once a slot is backed by storage.

The implementation is noexcept. Allocation failure is reported by
return value. On failure, previously allocated storage remains intact.

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.
- T must be non-const.
- Storage is uninitialised raw T[] memory.
- Indices, capacities, and alignment values are expressed in elements.

Scope:

- Models allocation topology and address mapping only.
- Does not track occupancy or liveness.
- Does not construct, destroy, or interpret T.
- Higher-level lifecycle and ownership belong to wrapper layers.

## Storage model

Storage is segmented into equal-sized buffers.

- Buffers are power-of-2 sized.
- Buffer directory is an array of memory::TMemoryToken<T>.
- Buffers are allocated lazily and never moved.
- Growth occurs by allocating new buffers.
- m_slot_capacity defines total covered slot range.

## Addressing model

Slot indices are mapped by decomposition:

    buffer_index = slot_index >> m_buffer_shift
    buffer_slot  = slot_index & m_slot_mask

Derived values:

- slots_per_buffer = m_slot_mask + 1
- slots_per_buffer is a power of 2
- buffer_index < buffer_count() implies allocated storage

## Status model

Status queries:

- is_valid performs full invariant validation including directory
  contents
- is_ready checks structural coherence without scanning directory
- is_empty reports fail-safe emptiness

Accessors are fail-safe and return nullptr when not ready or invalid.

## Constness model

Constness of TStableStorage does not imply immutable storage.

- Functions returning T* may be called on const instances.
- Immutable access must be provided by higher-level abstractions.

## Growth model

Growth occurs through buffer allocation.

- map_index ensures storage exists for a slot
- index_ptr does not allocate and may return nullptr
- Buffer directory grows geometrically (power of 2)
- Slot coverage grows in buffer-sized increments

## State model

Canonical empty:

    m_buffers.data() == nullptr
    m_buffer_capacity == 0
    m_buffer_shift == 0
    m_slot_mask == 0
    m_slot_capacity == 0

Ready state:

- Directory allocated
- Geometry coherent
- Slot coverage may be zero or non-zero

Covered range:

    [0, m_slot_capacity)

## Metadata invariants (ready state)

- m_buffer_capacity is power of 2 and > 0
- m_buffer_shift defines slots_per_buffer = 1 << m_buffer_shift
- m_slot_mask == slots_per_buffer - 1
- m_slot_capacity is a multiple of slots_per_buffer
- buffer_count() = m_slot_capacity / slots_per_buffer
- buffer_count() <= m_buffer_capacity

## Directory invariants (valid state)

- Buffers [0, buffer_count()) are allocated
- Buffers [buffer_count(), m_buffer_capacity) are empty

## Behavioural notes

- Mutating operations leave existing storage intact on failure
- No automatic compaction or shrinking is performed
- Intended as a low-level substrate for higher-level containers
