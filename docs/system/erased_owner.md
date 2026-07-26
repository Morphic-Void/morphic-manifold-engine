Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

File:   erased_owner.md
Author: Ritchie Brannan
Date:   26 Jul 2026

# CErasedOwner

## Purpose

`CErasedOwner` is the move-only system carrier for one registered payload whose
concrete type is known to the closed executable and module set. It is intended
for non-POD payload ownership and is distinct from `TErasedPod`, which stores
bounded trivially copyable message data inline.

The carrier owns direct payload storage through `CMemoryToken`. It contains no
virtual interface, destructor pointer, or other executable address that could
outlive a module. Destruction is selected in system-owned code by the generated
type ID.

The carrier layout is fixed at 32 bytes on x64 and 24 bytes on x86. The token
remains outside the allocation it may release.

## Registration

System payload identity is declared in `system/type_ids.def` and associated
with a C++ type through `MV_REGISTER_SYSTEM_TYPE`.

Owning-erasure eligibility is a separate explicit trait declared with
`MV_REGISTER_ERASED_OWNER_PAYLOAD`. A type ID does not by itself permit a type
to be carried by `CErasedOwner`.

Every eligible type must have an explicit destruction case in
`src/system/erased_owner.cpp`. Registration and lifetime handling therefore
remain a small, auditable closed-world operation.

Eligible payloads must be nothrow default constructible, move constructible,
move assignable, and destructible. They must also fit the memory-token stride
field.

## State And Lifetime

The canonical empty state has:

- type ID zero;
- empty token storage;
- hazard mask zero.

`create<T>()` allocates and directly placement-constructs `T`. Allocation
failure returns the canonical empty state.

Move construction and move assignment transfer the token without relocating
the payload, preserving the payload address. The moved-from carrier becomes
canonical empty. Move assignment first destroys any payload already owned by
the destination.

Ownership is singular and nominal. Only the owner may mutate the payload,
attribution, or hazards; access to the payload does not independently confer
ownership or concurrent mutation authority. The carrier contains no atomics.
Transport and access structures provide cross-thread synchronization and
ownership-transfer ordering.

`destroy()` selects the concrete destructor by type ID, destroys the payload,
deallocates the token, and restores canonical empty state. An unhandled nonzero
type ID is a critical architectural contract failure.

`payload<T>()` returns a pointer only when the registered type ID matches.

## Hazards

The carrier stores a non-atomic 32-bit mounting-point hazard mask. Singular
ownership and the surrounding communication structures provide the threading
contract; the nominal owner is responsible for mutation.

`add_hazard()`, `remove_hazard()`, and `has_hazard()` take strong mounting-point
IDs. The bit position is the generated zero-based mounting-point index.
`has_any_hazard()` provides the fast unload/destruction gate and
`hazard_mask()` provides diagnostic detail.

The build fails if the registered mounting-point count exceeds 32. The wider
runtime ID encoding capacity does not enlarge this carrier field.

Hazards describe module-lifetime dependency. They are not provisioning,
visibility, access-rights, or ownership metadata.

Hazards normally accumulate as work moves through the system. Removal is an
explicit owner-authorized safety transition after every dependency on the
mounting point has been relinquished. Moves preserve the complete mask and
reattribution does not alter it. There is intentionally no public whole-mask
setter.

## Reattribution

`can_reattribute_to()` preflights the carrier token and the registered
payload's nested ownership against a proposed target context.

`reattribute()` gathers the carrier and nested payload allocation count and
size, performs one accounting transaction, then replaces every participating
token context without further accounting. Carrier type, payload address, and
hazards remain unchanged. Empty owners succeed without becoming configured,
preserving canonical emptiness.

Current registered owner payloads contain either `CByteBuffer` or
`CByteRectBuffer`. Their context replacement hooks remain private and are
available only to the carrier transaction.

## Erased Owner Transport

`CErasedOwnerTransport` is the system-specific attribution-aware composition
over the non-reattributable `TOwning<CErasedOwner>` primitive.

On successful post:

- readiness, writable capacity, and source compatibility are checked before
  mutation;
- carrier and payload attribution move to the fixed transport context;
- ownership then moves into the underlying transport.

On successful read:

- ownership first moves out of the underlying transport;
- attribution then moves to the optional fixed recipient context;
- without a recipient context, transport attribution is retained.

Transport and recipient allocator compatibility is validated before
initialisation and ordinary operation. A read-time accounting failure still
delivers the item and triggers `MV_CRITICAL_ASSERT`; it is not treated as an
ordinary incompatibility or disposed of.

Thin producer and consumer endpoints expose only their role-specific wrapper
operations. The underlying `TOwning` and context mutation are not exposed.
Plain transports remain non-reattributable.

`MV_CRITICAL_ASSERT` marks failures that represent broken architecture or
accounting contracts. It currently aliases `MV_HARD_ASSERT`; the upcoming debug
infrastructure will provide published-build reporting and degraded-continuation
policy.

## Virtual Interface Boundary

Virtual classes remain valid internal implementation tools when their objects
and executable pointers cannot cross module lifetime boundaries. The
non-virtual design of `CErasedOwner` is specifically required because the
carrier may outlive or cross the module that supplied a payload. It is not a
codebase-wide prohibition on virtual dispatch.

The wider fence applies to any transported or retained vptr, callback,
deleter, function pointer, or operation descriptor whose defining module may
be unloaded. Such executable identity must not escape its valid module
lifetime.
