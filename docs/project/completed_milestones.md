Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

File:   completed_milestones.md
Primary draft: OpenAI tools
Reviewed and accepted by: Ritchie Brannan
Date:   26 Jul 2026

# Completed Milestones

## Purpose

This document is the concise project record for completed cross-cutting work.
It records outcomes and validation without retaining temporary implementation
plans, delegation instructions, snapshot guidance, or superseded decisions.

Permanent behavior and architectural contracts belong in the subsystem
documents linked from each milestone. Current and future work belongs in
`future_work_notes.md` and the backlog.

## Memory Ownership And Accounting

The memory refactor established one system-wide ownership and accounting model:

- raw allocation, alignment conditioning, accounting, and attribution live in
  `memory`;
- `CMemoryToken` provides relocatable and stable storage ownership;
- `CMemoryView` and `CMemoryConstView` provide bounded non-owning views;
- legacy tokens, views, stable-storage wrappers, allocation-context layers,
  and compatibility namespaces were removed;
- owning containers compose tokens rather than calling allocators directly.

Conditioned byte accounting, direct-storage statistics, and compatible-context
reattribution were completed across vectors, FIFOs, instances, byte and string
containers, POD slot containers, stable collections, and erased ownership.
Compound owners perform one aggregate accounting transaction for all direct
tokens. Storage-owning transports retain fixed attribution and remain
non-reattributable.

The explicit `bit_ops` casting audit was also completed. Platform-width
operands now select the platform-width overload directly; explicit conversions
remain only at genuine representation boundaries.

Permanent reference:

- `docs/memory/memory_subsystem.md`
- the individual documents under `docs/containers/`

Validation:

- Debug x64 and x86 solution builds;
- full memory, container, and transport suites on both architectures.

## Non-Virtual Slot Sandwich

The ordered and unordered slot families were converted from virtual
responsibility hooks to direct compile-time collaboration:

```text
container slot-backing base
    -> TOrderedSlots or TUnorderedSlots
        -> public container facade
```

The completed conversion covers:

- `TPodOrderedSlots`;
- `TPodUnorderedSlots`;
- `TOrderedCollection`;
- `TUnorderedCollection`;
- ordered and unordered test harness adapters.

Callback visitation and callback-specific locking were removed. Direct empty
traversal replaced the useful callback capability. Packing, metadata
copy/clone operations, traversal helpers, reserve behavior, ordered external
payload handling, equal-key stability, and stable collection object addresses
were preserved.

The public facade now owns user-facing lifetime orchestration while the middle
slot manager collaborates directly with protected lower backing
responsibilities. No container-hierarchy vtable or runtime responsibility hook
remains.

Permanent reference:

- `docs/containers/slots/TOrderedSlots.md`
- `docs/containers/slots/TUnorderedSlots.md`
- `docs/containers/TOrderedCollection.md`
- `docs/containers/TUnorderedCollection.md`

Validation:

- warning-free Debug x64 and x86 builds;
- focused memory, container, and transport suites;
- complete ordered deletion-permutation coverage during the conversion.

## System Identity And Erased Ownership

System transported types and generated IDs were moved into a system-owned
registration model. Owning-erasure eligibility remains separate from ordinary
type registration.

`CErasedOwner` replaced the former virtual typeless ownership mechanism with a
closed-world move-only carrier:

- one registered payload is placement-constructed in token-owned storage;
- component-local immutable tables dispatch erased destruction, accounting,
  validation, and reattribution for eligible SYSTEM and LOCAL payloads;
- SYSTEM operations are compiled into each receiving component while LOCAL
  operations remain only in their defining component;
- no vptr, callback, deleter, or executable operation pointer crosses a module
  lifetime boundary;
- payload addresses remain stable across owner and transport movement;
- the 32-bit mounting-point hazard mask is distinct from ownership,
  provisioning, and access metadata.

Payload and carrier reattribution use one aggregate accounting transaction.
`CErasedOwnerTransport` composes the non-reattributable `TOwning` primitive and
implements the fixed attribution chain:

```text
posting owner -> transport owner -> optional recipient owner
```

The inline POD message carrier moved to `system/erased_pod.hpp` as
`TErasedPod`, remaining distinct from non-POD erased ownership. Host prototype
messages and the owning host channel were migrated to the system surfaces.

Permanent reference:

- `docs/system/erased_owner.md`
- `docs/memory/memory_subsystem.md`

Validation:

- Debug x64 and x86 builds;
- 106 erased-owner and wrapper checks on each architecture;
- full test runs on both architectures.

## Bounded MPMC Transport

The fixed-capacity Rigtorp-style MPMC transport foundation is complete:

- `TMpmcIndexRing` supplies the status-free index-ring primitive;
- `TMpmcArenaTransport` supplies lifecycle-aware arena transport;
- `TMpmcJobTransport` composes work and feedback transports;
- scoped reserve and acquire helpers complete the mandatory protocol pairs;
- the family performs no live allocation and requires no internal attribution.

The implemented lifecycle is `open -> closing -> closed`, with forced
`shutdown`. Outstanding-index accounting prevents orderly closure while work
is reserved, published, or acquired but not yet recycled.

Legacy endpoint and bundle wrappers were consolidated into their core transport
headers and temporary compatibility shims were removed as part of the same
transport cleanup period.

Permanent reference:

- `docs/threading/transports/TMpmcTransport.md`
- the other documents under `docs/threading/transports/`

Validation:

- Debug x64 and x86 solution builds;
- focused capacity, sequence, wrap, lifecycle, protocol, helper, and
  composition coverage;
- full x64 and x86 test runs.

## Code Policy Validator

`MorphicPolicyValidator` established a standalone C++17 development-tooling
boundary for early detection of codebase policy violations. The validator may
use the C++ standard library and exceptions; production engine projects remain
C++17 with exception handling disabled.

The completed first version provides:

- a lightweight lexical and token-aware scanner rather than regex-only checks
  or a full C++ parser;
- central, versioned policy configuration for source scopes, include
  permissions, approved allocation infrastructure, identity surfaces, and
  project classifications;
- narrow source suppressions that name one rule, require a reason, and fail
  when malformed, unknown, unmatched, or stale;
- error-level checks for ordinary naked `new`, misplaced global/system identity
  declarations and registrations, disallowed or unresolved includes, direct
  `windows.h` use, required source roots, and Visual Studio language and
  exception settings;
- warning-level findings for context-dependent placement construction, raw
  allocation primitives, ambiguous includes, apparently unused `<new>`
  includes, macro-expanded includes, and unsupported project conditions; and
- distinct exit behavior for policy violations and invocation or policy-loading
  failures.

Host, Executive, and MorphicTests reference the validator project and invoke it
before compilation. Core remains per-consumer shared source through
`MorphicCore.vcxitems`; the validator is a separate executable rather than an
engine runtime component.

The validator is deliberately a policy canary rather than a semantic proof. It
does not preprocess source, build an abstract syntax tree, or infer arbitrary
manual reimplementations of policy-governed macro surfaces.

Permanent reference:

- `docs/project/policy_validator.md`
- `policy/morphic_policy.cfg`

Validation:

- clean direct validator runs with zero errors and warnings;
- successful Debug, Development, and Release solution builds for x64 and x86;
- exercised positive, negative, suppression, policy-loading, include-resolution,
  allocation, identity, and project-configuration cases.
