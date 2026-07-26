# Memory Refactor Handoff

Status: the token/view migration, downstream container migration, legacy
memory-layer removal, namespace promotion, and x64/x86 validation are complete.
This document records the surviving architecture and open follow-on work; the
source remains authoritative.

## Architecture

Raw allocation, deallocation, alignment conditioning, accounting, and
attribution belong to `memory`. Containers own or compose memory tokens and do
not call allocators directly.

The foundation consists of:

- `memory_policies.hpp`: byte ceiling, alignment, derived element limits, and
  growth policies.
- `memory_context.hpp/.cpp`: callback allocator boundary, attribution
  accounting, aggregate reattribution, and ambient memory contexts.
- `memory_token.hpp`: unified ownership for relocatable and stable storage.
- `memory_view.hpp`: bounded mutable and const views over contiguous strided
  memory.
- `system_context.hpp/.cpp`: the current home for ambient module, thread, and
  combined system IDs.
- `TBitField.hpp`: typed packed-field support.

`CMemoryToken`, `CMemoryView`, and `CMemoryConstView` are public members of the
`memory` namespace. The old tokens, views, allocation-context layer,
`TStableStorage`, and compatibility namespace have been removed.

## Token Contract

The token occupies 24 bytes on x64 and 16 bytes on x86. It stores a memory or
directory pointer, memory-context pointer, 32-bit requested count, 16-bit
stride, and 16-bit packed control. Configuration is immutable and includes
storage-alignment intent, storage mode, and stable per-buffer capacity.

The two modes are:

- Relocatable: contiguous storage that may move during resizing.
- Stable: segmented, non-contiguous storage whose existing element addresses do
  not move during growth.

`count()` is the requested user-visible capacity. Stable slack and directory
capacity are implementation details. `bytes()` is logical payload size and does
not include conditioning, stable slack, directory allocation, allocator
metadata, or platform overhead.

`storage_alignment()` reports normalized user intent rather than stronger
incidental alignment. No divisibility relationship is required between storage
alignment and stride. `element_alignment()` derives the recurring indexed
alignment from storage alignment and stride.

`data()` is available only for relocatable storage. `index_ptr()` is
mode-neutral. `allocate()` replaces storage in either mode, `reallocate()`
applies only to relocatable storage, and `grow_to()` applies only to stable
storage. Replacement paths allocate and validate new storage before mutating
the token.

Move construction and assignment transfer storage unconditionally. The source
retains its context, stride, alignment, mode, and stable-buffer configuration
while becoming empty and immediately reusable.

Clone has two forms:

```cpp
bool clone(const CMemoryToken& source) noexcept;
bool clone(const CMemoryToken& source, CMemoryContext* context) noexcept;
```

The first preserves the source context. The second overrides it; `nullptr`
selects the ambient context. Exact self-clone is a no-op. Self-clone to another
context creates replacement storage transactionally.

## Context And Accounting

`CMemoryAllocator` and `CMemoryContext` are non-copyable and non-movable. A
context never changes allocator. Allocator operations are private to the
context. Context compatibility is allocator object identity, allowing
attribution to change without changing physical allocation ownership.

Live allocation count and conditioned allocated bytes are relaxed atomic audit
telemetry, not synchronization. Stable accounting includes every complete
conditioned buffer and the conditioned allocated directory capacity.

`condition_alignment(requested_alignment)` and
`condition_bytes(conditioned_alignment, requested_bytes)` define canonical
conditioning. `memory::k_byte_size_ceiling` is the shared byte ceiling;
stride/type-specific count limits use the common derived-limit helpers rather
than local division.

Token and container reattribution reserve the complete allocation count and
conditioned-byte total in the target context before releasing it from the
source. Target reservation failure is recoverable and leaves the source
unchanged. Source release failure after reservation is an accounting-corruption
boundary: the target reservation is rolled back and the failure is reported as
critical.

Complete owning containers expose direct `memory_token_count()`,
`memory_allocation_count()`, and `memory_allocation_size()` statistics. Compound
owners gather a coherent source context from their storage-owning tokens,
perform one aggregate context transaction, and only then replace each token's
context through the explicitly unsafe non-accounting token operation. This is a
lightweight accounting and context-coherence check; it does not perform full
container semantic-integrity walks.

The layered slot containers keep the statistics, source-context gathering, and
post-accounting context replacement operations protected in their lower
metadata and backing-storage layers. The public complete facade corrals those
contributions and owns the aggregate transaction. Regular compound containers
perform the same aggregation directly over their members.

Ambient module and thread state is deliberately non-atomic. Provisioning must
not race live use. Each DLL requiring module-local ambient state must compile or
link its own module-local implementation rather than importing another module's
fallback state.

## Failure Boundary

- Allocation exhaustion from an otherwise valid allocator is recoverable and
  may be used speculatively.
- Invalid allocator state, missing callbacks, accounting corruption, and
  deallocation failure are critical conditions.
- The allocator callback reports deallocation failure to `CMemoryContext`.
  `CMemoryContext::deallocate()` consumes it; failure does not propagate through
  tokens or containers.
- Any policy preventing later allocation after a critical failure belongs in
  shared context or shutdown state, not distributed return handling.
- `MV_FAIL_SAFE_ASSERT` currently marks sites for future critical reporting.
  `MV_HARD_ASSERT` marks rollback failure where accounting is no longer sound.

## DLL Transfer Boundary

Reattribution is expected primarily when ownership moves from a DLL to the main
executable. Compatible allocation contexts are necessary but not sufficient for
safe transfer. Vtables, callbacks, deleters, function pointers, payload types,
and other module-local executable state must remain valid for the lifetime of
the transferred object.

Virtual dispatch has now been removed from the slot-container hierarchy. This
eliminated the container layer's principal DLL-local vtable dependency before
general container reattribution was exposed.

This is not a general prohibition on virtual interfaces. Internal virtual
mechanisms remain valid when their objects and vtables cannot escape or outlive
the defining binary. The cross-module hazard is stored executable identity:
vptrs, callbacks, deleters, function pointers, and operation descriptors
carried by data that may survive replacement or unloading of their defining
DLL. Performance and indirection remain ordinary local considerations.

Container reattribution covers only storage owned directly by the container.
It does not reattribute allocations owned by contained objects. Thread
transports are deliberately not reattributable. The current allocation-owning
transport families expose a read-only memory-context query and accept optional
explicit attribution during construction or initialise/configuration, defaulting
otherwise to the ambient memory context. That attribution remains fixed for the
configured transport lifetime; endpoints may observe it only through the owning
transport and must never alter it. Live endpoint and concurrency state makes
post-configuration transfer both unsafe and unnecessary. Closed-world erased
payload handling remains separate follow-on work.

## Open Work

1. Complete the strong system-ID and module mounting-point prerequisite needed
   by erased-owner hazards, attribution, and later provisioning.
2. Replace `CTypeless` with the system-owned `CErasedOwner` through the staged
   design in `../system/erased_owner_handoff.md`. The first checkpoint retains
   plain `TOwning` movement and defers payload/transport reattribution.
3. Add instantiation coverage for supported latent templates as they are
   touched. Do not classify an uninstantiated template as dead code by default.

The non-virtual ordered and unordered slot-container sandwich is complete. See
`../containers/slot_sandwich_refactor_handoff.md` for its final architecture,
functional fences, and validation record.

## Completed Follow-on Work

Direct-storage statistics and compatible-context reattribution are complete for
the owning vector, FIFO, instance, byte-buffer, string, POD slot, and stable
collection families. Public `memory_token_count()`,
`memory_allocation_count()`, `memory_allocation_size()`,
`can_reattribute_to()`, and `reattribute()` operations preserve object and
storage addresses and commit all direct tokens together. Empty tokens are
rebound with their owner after a successful transaction. Transports and
`CTypeless` remain intentionally excluded.

The `bit_ops` cast audit is complete. Calls with `std::size_t` operands now use
the overload set directly, selecting the platform-width implementation on x86
and x64. Decorative conversions through `std::uint64_t` and back to
`std::size_t` were removed from `memory_view.hpp` and `memory_token.hpp`.
Explicit conversions from the signed bit-index result to `std::size_t` or the
packed `std::uint16_t` representation remain because they communicate a real
representation boundary.

## Enduring Constraints

- `TOrderedCollection` and `TUnorderedCollection` require stable object backing.
  Their stable tokens use `sizeof(T)` stride, `memory::t_default_align<T>()`
  storage alignment, and the established floor-32 per-buffer hint. First
  mapping uses `map_index()` and later access uses `index_ptr()`; no contiguous
  view is requested.
- Slot metadata and slot-backing allocation preserve the existing cross-layer
  reserve behavior without coordinated rollback. Any transactional redesign
  remains a separate decision from the completed non-virtual conversion.
- `CTypeless` owns its token outside the erased allocation and ends the erased
  object lifetime before releasing storage. Preserve that lifetime safety in
  any descriptor-based redesign.
- A const owning container controls both container and pointee constness at its
  public surface. Mutable non-owning operations remain available through
  explicit mutable views.
- String containers intentionally distinguish no string from present-but-empty.
  Null storage means no string; non-null storage with zero logical length is a
  valid parser-visible state. Do not normalize this to the zero-count memory-view
  contract.
- Token mode predicates already establish configuration. Use
  `is_configured()` separately only for mode-neutral validation.

## Validation And Build

The completed migration and focused suites pass Debug builds on x64 and x86.
The exhaustive slot permutation tails are intentionally separate from focused
validation and may be stopped after the focused summaries when they are not the
subject of the workset.

Codex PowerShell sessions are not Visual Studio Developer shells. The current
Visual Studio installation is:

```text
C:\Program Files\Microsoft Visual Studio\18\Community
```

Normal project build:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
    .\ManifoldEngine.sln /m /p:Configuration=Debug /p:Platform=x64
```

Use `x86` for the 32-bit solution platform. For a narrow compiler invocation,
initialize and invoke the compiler in the same command because environment
changes do not persist between calls:

```powershell
cmd.exe /d /s /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul && cl.exe ...'
```
