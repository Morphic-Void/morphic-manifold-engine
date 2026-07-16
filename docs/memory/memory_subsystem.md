Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   memory_subsystem.md  
Author: Ritchie Brannan  
Date:   2 Jun 2026  

# Memory Subsystem

## Overview

The Manifold Engine memory subsystem defines the low-level allocation, ownership, view, and accounting contracts used by containers and related infrastructure.

The subsystem separates:

- allocation routing from allocation ownership;
- C++ ownership from accounting attribution;
- owning storage handles from non-owning views;
- byte extent from typed element count;
- trusted extent metadata from diagnostic ownership evidence;
- shallow accounting from explicitly documented deep accounting.

This is a cross-header policy document, not an API reference. Header TOF comments and local comments remain responsible for immediate type orientation, declaration-level notes, and narrow preconditions.

## Scope

The subsystem provides:

- nothrow raw byte allocation and deallocation entry points;
- shared allocation limits and growth policies;
- alignment normalization and typed default alignment;
- per-thread / per-module / per-DLL allocation context routing;
- local allocation accounting for leak detection and memory budgeting;
- move-only byte and typed storage tokens;
- byte and typed non-owning views;
- checked crossing points between byte and typed forms;
- move-only erased ownership for one typed payload-family node.

The subsystem is designed for C++17, no exceptions, and explicit failure handling.

## Non-goals

The memory subsystem does not provide general container semantics.

It does not provide bounds checking for views or raw memory access.

It does not construct, destroy, or relocate non-trivial object sequences in the raw token/view layer.

It does not make views responsible for extent, lifetime, ownership, or allocation attribution.

It does not perform deep accounting of nested containers unless a caller or container explicitly documents and implements that policy.

It does not make const wrapper objects imply immutable referenced memory. Read-only access is represented by const view types.

It does not make allocation-context counters live atomic telemetry. Context accounting is local accounting state unless a higher-level synchronization policy explicitly provides live observation.

## Layer model

The subsystem has four conceptual layers.

### Allocation context

The allocation context is the attribution and routing layer. It binds allocation requests to an externally owned allocator and records local allocation accounting for a thread/module accounting domain.

The allocation context owns accounting state, not heap storage.

### Allocation substrate

The allocation substrate provides mechanical helpers: shared limits, growth policies, alignment policy, allocator configuration, and nothrow byte/typed allocation entry points.

This layer provides allocation mechanics, not ownership semantics.

### Raw ownership and view primitives

The primitive layer defines byte and typed tokens for owning raw storage, and byte and typed views for observing or reinterpreting storage without owning it.

Tokens own storage and track extent. Views are non-owning and do not track extent.

### Erased typed-node ownership

The erased ownership layer provides a move-only carrier for one typed payload-family node. It performs typed construction, typed destruction, and checked erased recovery.

It is not a general container or multi-object ownership mechanism.

## Ownership boundaries

An owning token owns exactly the allocation it holds. Moving a token transfers C++ ownership to the destination token. Destroying a non-empty token releases its owned storage.

A view is only a reference to storage. Copying, moving, adopting, or deriving a view does not transfer allocation ownership and does not affect allocation accounting.

A typeless carrier owns one erased typed node. The carrier owns the node allocation and is responsible for destroying and deallocating it. Payload semantic emptiness, if any, belongs to the recovered payload type, not to the carrier.

Allocator interfaces and allocation contexts are not storage owners. An allocation context routes allocations and records accounting; the allocator object referenced by the context is externally owned and must remain valid for all allocations and deallocations routed through it.

## Allocation extent and accounting

The subsystem separates allocation identity from allocation footprint.

Allocation identity asks whether an object currently appears to own an allocation.

Allocation footprint asks how many bytes of allocation extent can be trusted and reported.

For owning memory tokens, owns_memory() is the allocation identity observer. It reports whether the owning pointer is non-null and is intentionally weaker than readiness.

For byte-footprint accounting, bytes() is the footprint observer. It is fail-safe and may report zero when extent metadata cannot be trusted.

A damaged token may therefore contain a non-null owning pointer while its alignment or extent metadata is invalid. In that case:

- owns_memory() may still indicate that allocation-count accounting is required;
- bytes() may report zero because the byte extent is not trusted;
- the combination is diagnostic evidence, not proof that the allocation is safe, complete, or accurately sized.

Container-facing accounting should use:

    allocation count  <- owns_memory()
    byte footprint    <- bytes()

For typed tokens, byte footprint is derived from element count and element size. The typed token owns storage interpreted as a tightly packed T[].

## Allocation-context role

An allocation context is the local accounting and routing authority for an allocation domain.

Typical domains are expected to correspond to a thread, module, DLL, or a controlled combination of those concepts.

The current allocation context tracks allocation counts. The intended accounting model also includes allocated byte footprint. When explicit accounting-transfer operations are added, they should adjust attribution when ownership crosses accounting domains.

The allocator pointer held by a context is non-owning. It must remain valid while the context can route allocations or deallocations through it.

Allocator replacement is only valid when the context has no recorded live allocations. This prevents outstanding deallocations from being routed through a different allocator domain.

is_usable() means the context currently has an allocator available for routing. It does not imply that the context is globally safe to inspect concurrently.

## Allocation routing

Raw byte allocation is routed through the active allocation mechanism.

The allocation substrate exposes byte allocation and deallocation entry points. These route to the active allocator/context layer.

Zero-size raw byte allocation is rejected and returns null.

The owning token layer uses a higher-level convention: requesting zero extent through token allocation/reallocation means "become empty". That operation deallocates existing owned storage and succeeds.

This layer difference is intentional:

    raw allocation layer:
        zero bytes is not an allocation

    owning token layer:
        zero extent is a request for canonical empty ownership

## Thread/DLL attribution

Allocation accounting is attributed to the allocation context that records the allocation.

A move of an owning token transfers C++ ownership. It does not, by itself, necessarily transfer accounting attribution.

When ownership crosses a thread, module, or DLL accounting boundary, accounting attribution must be transferred deliberately. When explicit accounting-transfer operations are added, the source accounting domain should stop reporting the transferred allocation and the destination accounting domain should begin reporting it.

The accounting-transfer policy should use token-side observers consistently:

    if owns_memory():
        transfer allocation count attribution

    transfer byte footprint using bytes()

If owns_memory() is true and bytes() is zero, allocation-count attribution can still be adjusted, but byte-footprint attribution cannot be trusted. That condition should be treated as diagnostic evidence of damaged or insufficient metadata.

Accounting transfer must not infer deep ownership. If a container owns nested containers, the outer container's shallow memory token accounts only for its own direct storage unless the container explicitly implements and documents recursive accounting.

## Threading and observation model

The ambient memory context is resolved from thread-local context first and then
module-local context. Context installation is provisioning state and must not
race live use.

`CMemoryContext` allocation count and allocated-byte counters are relaxed
atomics. They provide audit telemetry and accounting integrity checks, not a
general synchronization mechanism for the objects stored through a context.

## Byte ownership

Raw storage ownership is represented by `memory::CMemoryToken`.
A configured token records its memory context, element stride, storage-alignment
intent, requested count, and relocatable or stable mode. Relocatable storage is
contiguous; stable storage may be segmented. An empty token may retain its
configuration and context so it can be reused after a move or deallocation.

Byte-token observers are fail-safe. Pointer, alignment, and byte-count observers report canonical empty values when required metadata is not trusted. Allocation-count diagnostics use the allocation identity model described above.

Byte tokens are move-only. Moving transfers pointer, alignment metadata, and byte extent.

Byte-token cloning copies trivially copyable byte storage. Cloning an empty or broken-observed-as-empty source produces canonical empty destination ownership.

## Typed ownership

Typed ownership is represented by TMemoryToken<T>.

A typed token owns raw storage interpreted as a tightly packed T[] and tracks pointer and element count.

The canonical typed-token states are:

    empty:
        data == nullptr
        count == 0

    ready:
        data != nullptr
        count != 0

    broken:
        any other combination

Typed token alignment is derived from T.

Typed tokens do not construct, destroy, or relocate non-trivial element sequences. Typed reallocation and cloning require trivially copyable T.

A typed token's byte footprint is derived from:

    count * sizeof(T)

Typed tokens are move-only. Moving transfers pointer and count metadata.

## Byte versus typed ownership

Byte and typed ownership are two interpretations of raw allocation ownership.

Byte ownership is the lower-level form. It preserves byte extent and alignment intent.

Typed ownership preserves element count and derives alignment from the element type.

Crossing between byte and typed ownership is explicit and checked.

A byte token may steal ownership from a typed token directly. The resulting byte token records the typed token's pointer, typed alignment, and derived byte footprint.

A typed token may steal ownership from a byte token only when the byte token is valid and either empty or exactly compatible with T. Compatibility requires suitable alignment intent and byte extent.

Failed typed stealing leaves the destination unchanged.

Canonical empty state is preserved across successful stealing.

## Views and non-ownership

Views are non-owning references to storage.

A byte view carries pointer and guaranteed alignment.

A typed view carries pointer only.

Views do not carry allocation extent. Element count or byte extent must be supplied externally.

A view being ready does not imply that any particular range is safe to access. Bounds remain the caller's responsibility.

Byte views have canonical empty and ready states, plus broken states when pointer and alignment disagree.

Typed views have only empty and ready states:

    empty:
        data == nullptr

    ready:
        data != nullptr

Typed views have no separate broken state because they do not carry separate alignment or extent metadata.

Subview offset validation is the caller's responsibility. Byte subviews reduce guaranteed alignment according to byte offset. Typed subviews advance in whole T elements and preserve typed alignment.

## Constness

Wrapper constness applies to the wrapper object only.

A const memory token or const mutable view wrapper does not imply immutable referenced memory.

Read-only access is represented by const view types.

Mutable typed views may adopt mutable byte views/storage only.

Const typed views may adopt mutable or const byte views/storage.

## Alignment model

The allocation substrate applies an alignment policy before raw allocation.

The byte allocation alignment policy reduces the requested alignment to a power-of-two alignment and applies at least the pointer-alignment floor.

Byte tokens store normalized alignment intent.

Byte views report guaranteed alignment for the current address. This may be less than the actual physical alignment of the address, but it must not overstate the guarantee.

Byte subviews reduce guaranteed alignment based on byte offset.

Typed ownership and typed views derive alignment from T.

Typed ownership stealing from byte ownership requires exact compatibility. This preserves reasoning about allocation/deallocation alignment intent when ownership crosses between byte and typed forms.

## Extent model

Extent is carried only by owning tokens or by external caller context.

CMemoryToken carries byte extent.

TMemoryToken<T> carries element count and derives byte footprint.

Views are address/alignment references, not array objects and not bounds-checked spans.

Containers are responsible for maintaining their own logical size and capacity. Token extent usually corresponds to capacity allocation, not necessarily to logical element count.

## Reallocation semantics

Reallocation preserves exactly the caller-specified copy extent.

For byte ownership:

    reallocate(copy_bytes, new_bytes, align)

preserves exactly copy_bytes.

For typed ownership:

    reallocate(copy_count, new_count)

preserves exactly copy_count elements.

The copy extent must be valid for both the current and requested extents:

    copy_bytes <= min(current_bytes, requested_bytes)
    copy_count <= min(current_count, requested_count)

This is container-facing policy. Containers distinguish logical size from capacity, so reallocation must not implicitly preserve the full current allocation extent unless the caller asks for that.

Allocation and reallocation leave the current token state unchanged on failure.

A zero requested extent at the token layer deallocates existing storage and leaves the token canonical empty.

When zero_extra is true, the unpreserved suffix of the destination extent is zero-filled. This includes same-extent reallocations where the requested extent is unchanged but the preserved prefix is smaller than the extent.

## Deallocation metadata integrity

Deallocation requires correct metadata.

Current phase:

- owned-token deallocation uses MV_HARD_ASSERT to catch corrupt token metadata before deallocation;
- byte-token deallocation requires valid deallocation metadata whenever the owned pointer is non-null;
- typed-token deallocation of owned storage uses the owned pointer plus alignment derived from T.
 
Upcoming allocator-accounting phase:

- as lower deallocation signatures are extended for accounting, lower paths may duplicate metadata checks currently concentrated in token deallocation;
- typed element count becomes required for accounting and therefore becomes part of the metadata-integrity contract for accounting-aware deallocation.

Final phase:

- allocator-facing deallocation becomes the hard fatal boundary;
- corrupt allocator-facing deallocation metadata is routed through the debug fatal handler.

Fail-safe observers are for safe observation and diagnostics. They are not a license to silently deallocate with untrusted metadata.

If an owning pointer exists but required deallocation metadata is corrupt, the subsystem should prefer fatal diagnostic handling over undefined allocator interaction.

## Allocation failure and zeroing

Allocation functions are nothrow.

Raw allocation failure returns null.

Token allocation/reallocation failure leaves existing ownership unchanged.

Token allocation may optionally zero the entire new extent.

Token reallocation may optionally zero the unpreserved suffix. When reallocating from an empty source and zeroing is requested, copied-prefix-equivalent bytes may also be zero-filled so that the requested preserved region is deterministic.

Erased typed-node creation allocates storage for a typed node and placement-constructs the node. Payload types used with erased ownership must satisfy the nothrow construction, move, assignment, and destruction requirements imposed by the node wrapper.

## Container-facing accounting

Container allocation accounting is shallow unless explicitly documented otherwise.

For a container that directly owns one or more memory tokens:

    allocation count:
        count each token where owns_memory() is true

    byte footprint:
        sum bytes() for those tokens

A token with owns_memory() == true and bytes() == 0 should contribute to allocation-count diagnostics, but not to trusted byte-footprint totals.

For containers that can contain other containers, accounting is not automatically recursive. Recursive or deep accounting must be implemented and documented by that container.

Type-erased or typeless payloads need particular care. The erased carrier allocation is one direct allocation. The recovered payload may itself contain owning allocations. Unless documented otherwise, the carrier should account only for its own direct node allocation.

When ownership is transferred between containers, threads, or modules, accounting transfer must follow ownership transfer deliberately. Moving the C++ object is not sufficient if the accounting attribution domain changes.

## Container-facing reallocation

Containers should pass their logical preservation extent explicitly.

For byte-backed containers, this is usually the number of bytes corresponding to logical content, not capacity.

For typed containers, this is usually the logical element count, not capacity.

The token layer will not infer logical size from current allocation extent.

This avoids over-preserving stale capacity bytes and keeps reallocation semantics under caller control.

## Erased typed-node ownership

Erased typed-node ownership provides a move-only carrier for one typed payload-family node.

It is not a general container.

It is not a multi-object ownership mechanism.

Carrier emptiness means only that the carrier has no erased node. Payload semantic emptiness, if any, belongs to the recovered payload type.

Type identity is payload-family identity. Empty ownership reports type identity zero through the query API.

Typed recovery is explicit and checked through typeless_cast<T, type_id>(). A failed recovery returns null.

Typeless teardown destroys the typed node and then deallocates the externally owned token storage through the memory subsystem.

The subsystem-level accounting rule is that the carrier owns one direct allocation. Any ownership contained inside the recovered payload belongs to that payload's own accounting policy.

## Header map

The headers provide the following local surfaces:

- `memory_policies.hpp`: shared limits, growth helpers, and alignment policy.
- `memory_context.hpp`: callback allocator, ambient context routing, attribution,
  and allocation accounting.
- `memory_token.hpp`: relocatable and stable raw-storage ownership.
- `memory_view.hpp`: bounded mutable and const non-owning views.
- `memory_typeless.hpp`: move-only token-backed erased typed-node ownership,
  checked recovery by type identity, ordered typed-node destruction, and storage
  deallocation.

## Summary rules

Use owns_memory() for allocation-count diagnostics.

Use bytes() for trusted byte-footprint accounting.

Do not treat views as owners.

Do not infer extent from views.

Do not infer deep container accounting from shallow token ownership.

Do not infer accounting-domain transfer from C++ move alone.

Do not silently deallocate with corrupt metadata.

Preserve exactly the reallocation copy extent supplied by the caller.

Treat zero-size raw allocation and zero-extent token ownership as different layer policies.

Keep this document as the subsystem policy source. Keep headers compact and local.
