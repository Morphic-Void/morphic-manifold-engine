# Slot Sandwich Refactor Handoff

Status: complete. The preparatory stripping pass and non-virtual sandwich
conversion are implemented across all four production families and both test
harness adapters. This document records the design, functional fences, and
validation history of the completed work.

## Objective

Remove DLL-local vtable dependencies from the ordered and unordered slot
container families without discarding useful slot-management capabilities.
The intended hierarchy is a compile-time sandwich:

```text
container slot-backing base
    -> TOrderedSlots or TUnorderedSlots
        -> public container facade
```

The lower slot-backing base owns the container-specific state and provides
the primitive operations needed by the slot manager. The slot manager calls
downward through direct base calls. The public facade supplies the user-facing
API and coordinates object lifetime. Runtime subclass extensibility is not a
goal.

## Production Families

There are four direct production consumers:

- `TPodOrderedSlots`
- `TPodUnorderedSlots`
- `TOrderedCollection`
- `TUnorderedCollection`

The ordered and unordered test harness adapters are the only additional direct
derivations. No production base-pointer polymorphism, base-pointer deletion,
indirect derivation, or downcasting was found during the feasibility audits.

## Completed Preparation

The visit callback surface and callback-specific locking have been removed
from both slot managers:

- `on_visit`, `visit_*`, dispatchers, and visit-only test state are gone.
- `LockState`, `m_lock`, `lock`, `unlock`, `safe_on_*`, and lock assertions are
  gone.
- `is_safe()` remains as structural metadata-storage readiness validation.
- Slot-backing responsibility functions carry a documented no-re-entry
  contract rather than incomplete runtime enforcement.
- Empty slots now have direct `first_empty`, `last_empty`, `prev_empty`, and
  `next_empty` traversal, matching the existing loose traversal shape.
- Focused harness traversal tests cover both directions and category/count
  integrity.

This preparation is an architectural improvement rather than an accepted loss:
direct traversal preserves the useful capability while removing callback
control inversion.

## Functional Fences

Do not remove functionality merely because it has no current production caller.
In particular, preserve unless a concrete incompatibility is demonstrated:

- Ordered external-payload packing.
- Metadata-only copy, move, take, and clone facilities.
- Empty, loose, lexed, rank, and other facade-facing traversal helpers.
- Existing public facade APIs and observable ordering behavior.
- Existing non-transactional cross-layer reserve behavior during the hierarchy
  conversion. Any transaction redesign is a separate decision.

Ordered packing must retain the `-1` scratch-storage convention and stable
equal-key behavior. `TOrderedCollection` and `TUnorderedCollection` packing
must preserve object addresses. Constructors, destructors, and ordered key
comparison may still execute user code; removing virtual dispatch does not
remove those separate re-entry and DLL-lifetime considerations.

## Implemented Collaborations

The former virtual surface is now direct compile-time collaboration:

- Both families: payload movement during packing.
- Both families: payload/side-storage reserve coordination.
- Ordered family: key comparison.
- Both families: non-virtual destruction and correctly ordered shutdown.

The middle slot managers call `slot_backing()` to reach the lower layer's local
responsibilities. No runtime responsibility hooks or container-hierarchy
vtables remain.

## Lifecycle Constraints

The public facade orchestrates payload or object destruction before slot
metadata is released. The completed hierarchy has one clear non-virtual
ownership path rather than the former redundant shutdown path.

The collection slot-backing base remains alive whenever the middle slot
manager calls its movement, reserve, or comparison primitives. Pay particular
attention to this ordering in future construction, move-assignment, shutdown,
or destruction changes.

## Completed Conversion Sequence

The conversion was completed through coherent buildable checkpoints:

1. Define and name the lower slot-backing contract for the unordered POD
   family.
2. Convert `TPodUnorderedSlots`, update its harness adapter, and verify reserve
   and packing behavior.
3. Convert `TUnorderedCollection`, preserving stable object addresses and
   lifetime ordering.
4. Apply the established shape to `TPodOrderedSlots`, including comparison,
   duplicate detection, AVL operations, and external-payload packing.
5. Convert `TOrderedCollection`, preserving object addresses, equal-key
   stability, and `-1` scratch movement.
6. Remove the final virtual destructor and responsibility hooks once all four
   facades use direct collaboration.
7. Reassess the hierarchy surface while preserving metadata copy/clone and
   latent helpers under Chesterton's-fence discipline.

The resulting middle templates use `TSlotBacking` for the lower-layer type and
`slot_backing()` for direct collaboration. Backing primitives remain
inaccessible to ordinary container users without friendship or upward
references.

## Validation Baseline

The stripped virtual baseline passed:

- Debug x64 solution build.
- Debug x86 solution build.
- Full x64 test executable.
- All memory, container, and transport suites.
- Both ordered index/meta instantiations, including exhaustive ordered deletion
  permutations.
- Both unordered index instantiations and traversal/packing/reserve suites.

After the sandwich conversion and final naming pass:

- Debug x64 and x86 solution builds completed with no warnings or errors.
- All focused x64 memory, container, and transport suites passed.
- One complete 362,880-permutation ordered deletion set passed; the remaining
  exhaustive tail was not part of the final naming validation.

The Visual Studio installation and build commands are recorded in
`../memory/memory_refactor_handoff.md`. Use `x64` and `x86` as solution
platform names. The working tree intentionally contains the wider completed
memory/container migration; never revert unrelated changes.

## Delegation Procedure

This is a long-lived refactor with a large dirty working tree. Delegation is
useful, but workers must not inherit the coordinator's full history.

- Use `fork_context=false` by default and provide a compact task-specific
  handoff referencing this file.
- Use the shared checkout only for workers with strictly disjoint ownership of
  one to three files. Do not use worktrees merely to obtain a clean view of the
  intentionally dirty refactor.
- Prefer a short read-only explorer followed by a modest worker patch when a
  seam is uncertain. Do not repeat audits already captured here.
- Require workers to use direct `apply_patch`, stop after editing, and report
  changed files. The coordinator owns integration, broad builds, and tests.
- Interrupt and inspect a patch operation that has not completed within roughly
  two minutes.
- Never embed a large patch string inside a general execution wrapper.

The July 2026 delegation incident was not caused by Windows permissions or a
general inability to delegate. Two full-history workers stalled inside large
wrapped patch calls after inheriting an exceptionally large context. Minimal
workers with and without context forking both patched successfully; compact
handoffs and direct patch calls are the corrective procedure.

## Snapshot Record

Snapshots were retained at the stripped virtual baseline and at the coherent
unordered and ordered conversion checkpoints. The completed sandwich is the
new buildable baseline.
