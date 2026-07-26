# MPMC Transport Handoff

Status: initial implementation checkpoint landed and validated on Debug x64/x86;
legacy transport wrapper refactor and shim cleanup also landed and validated on
Debug x64.

This document is the authoritative record for the bounded MPMC transport task.
It captures the agreed objective, structure, invariants, lifecycle semantics,
layout constraints, validation targets, and implementation checkpoints for the
new single-header transport family.

Follow-on cleanup also consolidated the older transport endpoint and bundle
wrappers into their core transport headers and removed the temporary
compatibility shim headers once downstream includes had been updated.

## Objective

Add a bounded fixed-capacity MPMC transport family for upcoming debug
infrastructure and later job-system use.

Unlike the existing SPSC transports, this family is:

- fixed-size through template parameters rather than runtime allocation
- intended to live in a single self-contained template header
- built as a small stack of closely related templates rather than separate
  core/composed files
- movable or reattributable only through `TInstance<...>` ownership of the
  whole instantiated object, not through transport-internal allocations

The transport family must remain allocation-free during live transport
operations.

Because the family owns no allocator-managed internal storage, it does not
participate in the transport memory-attribution query surface added to the
allocation-owning SPSC transports. Attribution for MPMC use remains external to
the transport object itself, for example through an owning `TInstance<...>`
wrapper when one is used.

## Agreed Family Structure

The family consists of three layers.

### 1. Raw index MPMC building block

The base building block is a Rigtorp-style bounded MPMC ring carrying arena
indices.

Properties:

- fixed compile-time capacity hint conditioned to a compliant power-of-two
  capacity
- no status word
- no allocator use
- no transport policy beyond non-blocking push/pop
- intended as a reusable composition primitive rather than a standalone
  transport family endpoint

Each slot consists of:

- `std::atomic<std::uint32_t> sequence`
- `std::uint32_t payload`

Slot payload values are initialised monotonically from `0` upward.

Construction accepts a `bool` selecting whether the ring starts logically empty
or logically full.

### 2. Arena-backed bidirectional MPMC transport

The first real transport composes:

- one initially full raw index MPMC supplying and recycling arena indices
- one initially empty raw index MPMC transporting populated arena indices
- one arena whose element count matches the transport capacity

The supplier/recycler ring holds the set of currently available arena indices.
The populated-work ring holds indices whose arena elements contain committed
work.

The protocol is:

- producer side: `reserve -> populate -> publish`
- consumer side: `acquire -> process -> recycle`

The transport exposes arena-slot pointers on successful `reserve()` and
`acquire()`.

### 3. Paired higher-level transport

The next composition layer contains two arena-backed transports with distinct
types and capacities:

- one for job supply
- one for completion or status return

This paired layer is primarily compositional and should not invent new queue
mechanics unless later work proves the need.

## Capacity Rules

The raw index MPMC capacity is determined from a template hint.

Rules:

- the effective capacity is rounded up to the nearest compliant power of two
- minimum effective capacity is `16`
- maximum effective capacity is the smallest power of two greater than
  `1,000,000`
- a hint larger than the allowed maximum is a compile-time error

The effective maximum therefore evaluates to `1,048,576`.

## Layout And Isolation Constraints

The design deliberately treats `128` bytes as the effective cache-line
isolation boundary for the critical atomics and storage regions.

The following regions must each begin at `alignas(128)` and must not overlap
one another within the containing object:

- the raw index MPMC slot array start
- the raw index MPMC producer position atomic
- the raw index MPMC consumer position atomic
- the arena transport lifecycle status atomic
- the arena transport outstanding-count atomic

The status atomic and outstanding-count atomic must not share a cache-line
region with:

- each other
- either read/write index atomic
- the raw slot array
- the arena element array

The slot array alignment requirement applies to the start of the array, not to
every slot.

## Raw Index MPMC Semantics

The raw index MPMC is status-free and mechanically narrow.

It provides:

- `push(payload, out_sequence)`
- `pop(out_payload, out_sequence)`

Both operations:

- are non-blocking
- use simple success/fail `bool` returns
- follow try-style operational semantics despite the plain names

On successful `push()`, the sequence value returned through the out parameter
must match the sequence later returned by `pop()` when that same payload is
removed.

The raw ring is initialised either:

- logically empty, or
- logically full with payload values `0..capacity-1`

The initially full mode exists to seed the supplier/recycler side of the
arena-backed transport.

The raw ring does not carry lifecycle state and does not independently earn a
status-bearing variant at present.

## Arena Transport Lifecycle

The arena-backed transport carries lifecycle state. The agreed public states
are:

- `open`
- `closing`
- `closed`
- `shutdown`

`failed` was considered and intentionally dropped. Normal full/empty rejection
is not failure, caller protocol abuse is generally not reliably detectable in
real time, and memory corruption is outside the scope of a meaningful runtime
transport-local failure state.

### Transition Model

- `open -> closing`: explicit orderly close request
- `closing -> closed`: automatic transition when the final outstanding arena
  index is recycled
- `open -> shutdown`: explicit forced-stop request
- `closing -> shutdown`: explicit forced-stop request during drain

`closed` and `shutdown` are terminal.

### Operation Admission

In `open`:

- `reserve()` may succeed
- `publish()` may succeed for previously reserved slots
- `acquire()` may succeed
- `recycle()` may succeed

In `closing`:

- new `reserve()` calls fail immediately
- `publish()` continues for already reserved slots
- `acquire()` continues for already published slots
- `recycle()` continues for already acquired slots

In `closed`:

- all operations fail

In `shutdown`:

- all operations fail

`shutdown` is intentionally distinct from orderly close. It is the transport
authority's immediate-stop state and may be useful during controlled or
uncontrolled application shutdown.

## Outstanding-Count Semantics

The arena transport requires an additional isolated atomic outstanding counter.

Purpose:

- prevent `closing` from mistaking "currently no published work" for "all
  arena indices have returned home"

The outstanding count includes every index that has left the supplier/recycler
home side and has not yet been recycled back.

This includes:

- reserved but not yet published indices
- published but not yet acquired indices
- acquired but not yet recycled indices

This implies:

- a successful `reserve()` increments outstanding count
- a successful `recycle()` decrements outstanding count
- `closing -> closed` occurs only when a successful recycle returns the final
  outstanding index

## Irrevocable Caller Contract

Successful acquisition steps are irrevocable commitments.

Rules:

- a successful `reserve()` must eventually be followed by `publish()`
- a successful `acquire()` must eventually be followed by `recycle()`
- there is no speculative reserve/acquire path
- there is no cancellation or rollback path
- completion timing is unconstrained, but paired completion is mandatory

The transport therefore does not need undo semantics for admitted work during
`closing`.

## Courtesy Scoped Wrappers

The family may include courtesy RAII wrappers around the arena transport
protocol.

Current direction:

- a reserve wrapper acquires a reserved arena slot in its constructor
- the caller writes directly into the exposed payload
- the reserve wrapper publishes in its destructor if it owns a live reserved
  slot
- an acquire wrapper acquires a populated arena slot in its constructor
- the caller reads or processes through the exposed payload
- the acquire wrapper recycles in its destructor if it owns a live acquired
  slot

These wrappers are intended as convenience protocol helpers, not rollback
objects.

Open caveat:

- the user explicitly noted some remaining caveats about wrapper behavior, so
  wrapper details should be kept local and easy to revise while the core
  transport semantics settle

## Ownership And Moveability

The raw index ring and the composed transports should perform no live
allocation.

Moveability or reattribution, where required, is expected to come from wrapping
the whole instantiated transport object in `TInstance<...>`.

Consequences:

- the transport family itself does not own allocator-managed internal storage
- the object may be wrapped in a relocatable single-instance owner
- live shared transports should still be treated as practically non-movable
  once endpoints or external references are in use

## File Structure

This new family should be implemented in a single self-contained template
header rather than following the current split-file transport pattern.

The expected contents of that header are:

- raw index MPMC building block
- arena-backed transport
- paired higher-level transport
- related status enum and small helper types
- optional scoped courtesy wrappers if included in the first pass

The existing SPSC transports may later be refactored toward a similar
single-file pattern and may adopt parts of the lifecycle/status pattern
established here.

## Validation Targets

The new family needs focused validation for:

- capacity conditioning and compile-time limits
- initially empty and initially full raw ring construction
- exact sequence identity preservation across push/pop
- repeated producer and consumer index wrap
- exact-once arena-index circulation
- reserve/publish/acquire/recycle pipeline correctness
- closing rejecting reserve while allowing committed work to drain
- `closing -> closed` only after the final recycle returns the last outstanding
  index
- shutdown rejecting all operations immediately
- sustained MPMC contention
- repeated construction and teardown through owning wrappers where applicable

Windows validation is the immediate target. Linux stress and sanitizer coverage
remains later follow-on work when that build path is practical.

## Planned Implementation Order

1. Add this handoff/spec document.
2. Implement the raw index MPMC and arena transport in a new single header with
   buildable checkpoints.
3. Add the paired higher-level transport and any small helper wrappers that are
   justified by the settled semantics.
4. Add focused tests.
5. Wire the new header into the transport include surface.
6. Run focused builds/tests and update backlog status when the task state
   materially changes.

## Files Expected To Change

The exact set may expand, but the current expectation is concentrated in:

- `docs/threading/mpmc_transport_handoff.md`
- one new header under `include/threading/transports/`
- one or more focused test sources under `src/tests/`
- transport convenience includes if needed

## Files Changed In This Checkpoint

- `docs/threading/mpmc_transport_handoff.md`
- `include/threading/transports/TMpmcTransport.hpp`
- `include/threading/transports/transports.hpp`
- `include/tests/TMpmcTransport_test_suite.hpp`
- `src/tests/TMpmcTransport_test_suite.cpp`
- `src/tests/run_tests.cpp`
- `ManifoldEngine.vcxproj`
- `ManifoldEngine.vcxproj.filters`

## Implemented In This Checkpoint

The first buildable implementation includes:

- the status-free raw `TMpmcIndexRing<t_capacity_hint>`
- the lifecycle-aware `TMpmcArenaTransport<T, t_capacity_hint>`
- the compositional `TMpmcJobTransport<...>`
- courtesy RAII reserve and acquire wrappers whose destructors publish and
  recycle respectively
- include-surface wiring through `threading/transports/transports.hpp`
- a focused standalone test suite covering the new family

The current implementation uses a persistently live arena array and therefore
requires `T` to be nothrow default constructible and nothrow destructible.
That constraint is an implementation choice in this checkpoint rather than a
fully closed long-term architectural decision.

## Remaining Open Decisions

- final public names for the new template family
- exact courtesy-wrapper API and whether it ships in the first implementation
  pass
- whether the paired higher-level transport earns any additional coordinated
  lifecycle surface beyond composition of its two arena transports

## Validation Record

This checkpoint has passed:

- Debug x64 solution build
- Debug x86 solution build
- full x64 test executable run including the new `TMpmcTransport` suite

The focused new suite currently covers:

- compile-time capacity conditioning
- initially empty and initially full raw ring state
- sequence identity preservation through push/pop
- repeated wrap-oriented push/pop reuse
- arena reserve/publish/acquire/recycle flow
- orderly `closing -> closed` drain behavior
- immediate close while idle
- forced `shutdown` rejection
- courtesy RAII reserve/acquire wrappers
- paired transport composition sanity

## Remaining Work

- decide whether the current public type names are the settled names
- revisit the courtesy-wrapper surface now that the core protocol is in code
- add stronger contention and race-oriented tests for the new family
- decide whether the live arena should remain default-constructed storage or be
  reworked toward a different object-lifetime model
- determine whether any additional public queries or diagnostics are needed
  before the debug-system consumer is built on top
