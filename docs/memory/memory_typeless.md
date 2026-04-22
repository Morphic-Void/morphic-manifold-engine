Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   memory_typeless.md  
Author: Ritchie Brannan  
Date:   22 Apr 2026  

# memory_typeless

## Overview

This document defines typeless ownership utilities implemented in
memory_typeless.hpp.

The layer provides move-only erased ownership for one heap-allocated
typed payload-family node.

`CTypeless` is the public owner abstraction. `TTypeless<T, type_id>` is
the thin typed implementation node behind that owner.

This layer belongs to the memory and ownership substrate, not to the
container family.

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.

Scope:

- Models unique ownership of one erased typed node.
- Provides checked typed recovery by payload-family ID.
- Does not model multi-object ownership.
- Does not provide container semantics.
- Does not provide a general runtime type system.
- Does not provide an open-ended registration framework.
- Does not provide a plugin extension mechanism.

## Owner model

`CTypeless` is a move-only erased owner.

Carrier state:

- empty means `m_typeless == nullptr`
- ready means `m_typeless != nullptr`

`operator bool()` mirrors ready state.

At the `CTypeless` level:

- null and non-null states are both valid
- no meaningful `is_valid()` function is required

Carrier emptiness is not payload semantic emptiness.

A non-empty `CTypeless` owns exactly one heap-allocated typed node
implementing `ITypeless`.

## Type identity model

`type_id()` reports payload-family identity.

- empty ownership reports `0`
- non-empty ownership reports the fixed payload-family ID of
  the owned node

The reported ID is not wrapper implementation identity.

Payload-family IDs are part of a closed shared protocol across the
executable and participating DLLs.

## Payload model

`TTypeless<T, type_id>` contains an always-live default-constructed
payload object of type `T`.

The typeless layer does not maintain a separate semantic occupancy
state for that payload.

Payload semantic emptiness, if any, is determined by the payload
type itself.

Typeless-compatible payload types are expected to support:

- default-empty construction
- consumed-to-empty move semantics

Replacing payload content is by move-assignment into the always-live
payload object.

Typed semantics remain with `T`, not with the typeless layer.

## Typed recovery model

Typed recovery is explicit and checked.

Recovery functions:

- `typeless_cast<T, type_id>(CTypeless&)`
- `typeless_cast<T, type_id>(const CTypeless&)`

Recovery result:

- success returns `T*` or `const T*`
- failure returns `nullptr`

Successful recovery requires exact `type_id` match.

Typed operations occur through the recovered `T` pointer, not through
`TTypeless` itself.

## Lifetime and destruction model

`CTypeless` owns and destroys the typed node through
`ITypeless::destroy_and_deallocate()`.

`create<T, type_id>()`:

- allocates raw storage for `TTypeless<T, type_id>`
- placement-constructs that node in the allocated storage
- default-constructs the payload as part of node construction

The payload remains live until:

- `destroy_and_deallocate()`, or
- wrapper destruction

Final node teardown:

- destroys the full `TTypeless<T, type_id>` object
- deallocates its storage using the node alignment

The explicit destructor call in `TTypeless` is intentional. It marks the
actual lifetime boundary and preserves correctness if the node becomes
more non-trivial in the future.

## ITypeless protocol

`ITypeless` is intentionally minimal.

Current protocol surface:

- `destroy_and_deallocate() noexcept`
- `type_id() const noexcept`

This layer is not intended to grow into a broader runtime
polymorphism framework.

## TTypeless role

`TTypeless<T, type_id>` is the implementation node behind `CTypeless`.

It binds:

- payload type `T`
- payload-family ID `type_id`
- erased destruction protocol

It is not intended as a standalone public owner type.

It is not intended for stack use.

It should remain extremely thin and should not grow container-like or
framework-like responsibilities.

## Type requirements

`TTypeless<T, type_id>` requires `T` to be:

- nothrow default constructible
- nothrow move constructible
- nothrow move assignable
- nothrow destructible

These requirements are part of the intended typeless payload contract.

## Shared IDs and higher-level binding guidance

A lightweight shared header should define payload-family IDs.

That header should:

- contain constexpr IDs only
- be the ground truth for payload-family identity across the executable
  and participating DLLs
- remain lightweight and broadly includable
- avoid including concrete payload types
- define IDs only, not ID-to-type mappings

The core typeless mechanism remains generic over `<T, type_id>`.

`memory_typeless.hpp` should not require a global `T -> type_id`
mapping to function.

If desired, a higher-level convenience layer may provide:

- trait specializations
- local aliases
- `T`-only wrappers built on top of the explicit core forms

That convenience layer should remain optional and external to
the core substrate.

## Recommended layering

1. Shared protocol IDs header
   - constexpr IDs only
   - no concrete payload type includes

2. Core typeless substrate
   - `CTypeless`
   - `ITypeless`
   - `TTypeless<T, type_id>`
   - `create<T, type_id>()`
   - `typeless_cast<T, type_id>()`

3. Optional convenience bindings
   - trait specializations or local aliases near concrete types
   - `T`-only wrappers built on top of the explicit core forms
   - optional and external to the core mechanism

## Architectural placement

This facility belongs with the memory and ownership substrate.

It is intended to support controlled typeless ownership transfer and
typeless registry storage in a closed multi-threaded, multi-DLL system.

This is not a plugin ABI mechanism.

## Non-goals

- not a general runtime type system
- not an open-ended registration framework
- not a plugin extension mechanism
- not a container abstraction
- not multi-object ownership
- not a replacement for the explicit container family

## Terminology notes

Prefer:

- owner
- carrier
- payload-family identity

Distinguish clearly between:

- carrier emptiness: `CTypeless::is_empty()`
- payload semantic emptiness: payload-defined, such as `T::is_empty()`
