Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TPodVector.md  
Author: Ritchie Brannan  
Date:   1 Apr 2026  

# TPodVector<T>

## Overview

This document defines POD vector and typed view utilities implemented
in TPodVector.hpp.

The layer provides contiguous tightly packed storage for trivially
copyable elements. It includes owning vector storage and non-owning
typed views.

The implementation is noexcept. Allocation failure is reported by
return value. Accessors are fail-safe.

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.
- T must be non-const and trivially copyable.
- Storage is tightly packed contiguous T elements with no
  per-element padding.
- Sizes, capacities, and indices are expressed in elements.

Scope:

- Models contiguous tightly packed element storage only.
- Does not perform construction or destruction.
- Does not support non-trivial relocation semantics.
- Higher-level element meaning belongs in wrapper layers.

## Memory model

Ownership is provided by memory::TMemoryToken<T>.

- storage is interpreted as tightly packed T[]
- size() is the logical element count
- capacity() is the allocated element count
- spare capacity is not part of the logical range

## Growth model

Automatic growth uses memory::vector_growth_policy().

Operations:

- reserve(minimum_capacity) ensures capacity >= minimum_capacity
- ensure_free(extra) ensures at least extra spare elements
- shrink_to_fit() reduces capacity to match size

## Reallocation model

Reallocation preserves only the logical element range.

- preserved range: [0, size)
- shrinking truncates to new size
- spare capacity contents are not preserved
- storage beyond logical range is uninitialised
- newly exposed elements may be zeroed where required

## Alignment model

- storage is tightly packed T[] with stride sizeof(T)
- alignment is derived from T via t_default_align<T>()
- alignment is not configurable per instance

## Observation model

- accessors are fail-safe
- observers reflect container invariants
- size == 0 reports empty even if capacity != 0
- size == 0, capacity != 0 is a valid ready state

## Typed storage invariants

Canonical empty:

    data == nullptr
    size == 0
    capacity == 0

Valid:

    data != nullptr
    size <= capacity
    capacity != 0
    storage is tightly packed T[]
    alignment is sufficient for T

## Initialisation model

- resize(size) grows logical range and zeroes new elements
- push_back_zeroed / insert_zeroed expose zeroed storage
- push_back_uninit / insert_uninit expose uninitialised storage
- reserve / ensure_free may grow capacity without changing size

Zeroed growth is byte-zeroing only.

## Copy and representation model

- element transfer uses byte-wise copy or move
- padding bytes may be propagated
- byte-level equality is not guaranteed for semantically equal values
