Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TQueueTransport.md  
Author: Ritchie Brannan  
Date:   15 Apr 2026  

# TQueue

## Overview

`TQueue<T>` is a single-producer, single-consumer transport for
trivially copyable typed elements.

It is designed for sequential transport rather than shared random
access. The producer appends elements and publishes buffer states
through a compact atomic staging word. The consumer reads accepted
buffers from the start and never partially adopts a new buffer.
Communication is asymmetric:

- the producer owns publication and buffer-role transitions
- the consumer owns read position within its currently accepted buffer
- staged publication is transferred through a single atomic staging word
- phase is used as part of the consumer acceptance protocol

The transport uses three logical buffers and supports two capacity
policies:

- fixed-capacity mode, optionally with producer-side discard of pending
  producer-managed backlog
- growable mode, where producer-managed buffers may grow up to a
  configured maximum

Allocation failure during producer-side reallocation is treated as a
terminal producer-side fault. In that state the producer is poisoned
and further `post()` operations fail. Data already published remains
valid for consumer-side reading.

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.
- `T` must be non-const.
- `T` must be trivially copyable.

`TQueue<T>` provides:

- SPSC transport of trivially copyable `T`
- single-element and bulk `post()`
- single-element and bulk `read()`
- all-or-nothing bulk semantics
- producer-side publication through staged buffer exchange
- consumer-side phase-gated acceptance
- optional producer-side discard policy in fixed-capacity mode
- optional producer-side growth in growable mode
- producer-side `post_would_reallocate()` query
- consumer-side `current_readable_count()` and
  `refresh_readable_count()`

`TQueue<T>` does not provide:

- MPSC, SPMC, or MPMC semantics
- blocking or waiting semantics
- general-purpose queue semantics across arbitrary threads
- in-transport lifecycle arbitration between producer and
  consumer wrappers
- recovery from allocation failure
- preservation of all pending producer-side backlog when
  discard policy is enabled

## State model

The transport uses three logical buffers.

At any time the producer tracks three producer-side roles:

- output buffer: producer-owned writable accumulation buffer
- staged buffer: most recently published producer-managed buffer
- locked buffer: buffer assumed to be owned by the consumer side

The consumer tracks:

- current consumer buffer index
- current read position within that buffer
- expected phase for the next accepted staged publication

The producer tracks:

- current producer phase
- output, staged, and locked role indices

A single atomic staging word transfers publication state.

The staging word uses a compact encoded form:

- `0` means no staged publication is currently present in the word
- low bits encode published buffer identity
- bit `4` encodes publication phase

The transport also maintains configuration state:

- `m_capacity`: current active producer-side working capacity
- `m_max_capacity`: configured maximum capacity and readiness marker
- `m_allow_discard`: fixed-capacity discard policy enable
- `m_allocation_failed`: terminal producer-side allocation failure state

## Observation model

### Producer-side observations

Producer-side observation is limited to producer-owned state plus the
staging word.

The producer may inspect and mutate:

- producer role indices
- producer phase
- producer-managed buffer sizes and capacities
- current active capacity
- staging word by atomic exchange

The producer must not rely on consumer-owned read position or other
consumer-local state.

### Consumer-side observations

Consumer-side observation is limited to consumer-owned state plus the
staging word.

The consumer may inspect and mutate:

- current consumer buffer index
- current read index
- consumer phase
- staging word by atomic exchange

The consumer does not attempt to reconstruct hidden producer internal
history beyond what is represented by the acceptance protocol itself.

### Quiescent observation

Full structural validation requires quiescence. Quiescent validation
may inspect:

- all buffers
- all role indices
- all phases
- all capacities and sizes
- staging word state

Quiescent validation is stronger than in-flight role-specific validity
checks.

## Capacity model

Configured capacities are treated as minimum requested capacities, not
necessarily exact internal capacities.

Internal working capacities may be conditioned by implementation
policy, including:

- minimum capacity guarantee
- growth-policy conditioning

In fixed-capacity mode:

- `m_capacity == m_max_capacity` after initialisation
- no producer-side growth occurs
- discard may be enabled

In growable mode:

- the initial working capacity may be lower than the configured maximum
- producer-side posting may increase `m_capacity` up to
  `m_max_capacity`
- discard is not enabled

The consumer does not own capacity changes. Producer-side growth may
leave producer-managed buffers transitioning toward the new capacity
over time.

## Publication model

Each producer `post()` appends new elements to the current output
buffer and then publishes that buffer through the staging word.

There are two main publication outcomes.

### Previously staged publication not yet consumed

If the producer publishes and the exchange does not return zero:

- the previously staged publication had not yet been consumed
- publication remains in the current producer phase
- producer-side pending backlog is continued
- the staged producer-managed backlog is extended with the new posting
- output and staged roles are rotated

### Previously staged publication already consumed

If the producer publishes and the exchange returns zero:

- the previously staged publication had already been consumed
  by the consumer
- the just-published current-phase buffer is treated as obsolete
  for consumer progression
- the producer flips phase
- the producer republishes a canonical rebased buffer for the new phase
- the republished buffer contains only the new posting
- producer-side role indices are updated accordingly

This is the rebase path.

## Consumer acceptance model

The consumer interacts with the staging word only through the normal
acceptance protocol.

The consumer maintains an expected phase.

When the consumer needs a new readable buffer,
`refresh_readable_count()` exchanges the staging word with zero and
interprets the returned publication:

- zero: no staged publication available
- matching expected phase: publication accepted and consumer phase
  toggles
- mismatched phase: no currently acceptable readable buffer is acquired

Clearing the staging word during this exchange is intentional protocol
processing, even when the returned phase is not currently acceptable.
The staging word is not treated as a passive mailbox to be preserved
for later re-reading. It is an active handoff point in the producer and
consumer protocol.

Accepted consumer buffers are always read from the start. The consumer
never partially adopts a new buffer.

## Discard policy

Discard applies only to producer-managed pending backlog.

Discard does not revoke or mutate:

- consumer-owned current buffer
- elements already accepted by the consumer

When discard is enabled and growth cannot make the new posting fit:

- producer-side pending backlog may be abandoned
- producer-managed pending buffers are reset to begin again from the
  new posting
- consumer-owned data already acquired remains valid and readable

Discard is intentionally coarse. It is a pressure policy for this
transport and does not attempt to preserve a newest suffix subset of
pending producer backlog.

## Allocation failure and poisoning

Producer-side reallocation failure is treated as a terminal
producer-side fault.

When producer-side allocation failure occurs:

- `m_allocation_failed` is set
- further `post()` operations fail
- `post_would_reallocate()` reports false
- producer-side readiness and validity fail
- consumer-side already published data remains valid
- consumer may continue draining accepted and staged data according
  to the normal protocol
- the transport instance should be retired or deallocated by the
  owner layer

The transport does not attempt to recover from allocation failure.

## Post semantics

`post()` is all-or-nothing at the API boundary.

A posting succeeds only if the entire requested count can be accepted
under the active capacity policy.

A `post()` fails if:

- the producer is poisoned
- the transport is not initialised
- `count > m_max_capacity`
- `src == nullptr` and `count != 0`
- fixed-capacity mode cannot accept the request and discard
  is not allowed

`src == nullptr` with `count == 0` succeeds when the producer
side is ready.

The view overload requires a valid view. A zero-length invalid view fails
rather than being treated as a pointer-count zero operation.

A `post()` may also fail after partial internal progress if a
producer-side reallocation fails. In that case:

- the producer becomes poisoned
- previously published data remains valid for the consumer
- the instance is no longer considered healthy for further producer use

This is an intentional terminal-fault policy.

## Reallocation query semantics

`post_would_reallocate(count)` is a producer-side advisory query.

It reports whether a successful `post(count)` would require reallocation
of the current producer output buffer under the current policy view.

It is not a reservation mechanism.

`false` does not guarantee that a later `post(count)` will avoid reallocation.

The query returns false when:

- the producer is poisoned
- the transport is not initialised
- `count == 0`
- `count > m_max_capacity`
- policy would reject the posting

## Read semantics

`read()` is all-or-nothing.

For a bulk read to succeed:

- the requested count must be available in the current accepted
  consumer buffer after any required protocol refresh
- otherwise the read fails and consumer state is not advanced
  by that read

Reads always consume from the current accepted consumer buffer
in sequence.

The view overload requires a valid view. A zero-length invalid view fails
rather than being treated as a pointer-count zero operation.

When a read exhausts the current consumer buffer:

- the consumer buffer is released
- consumer ownership returns to the null-buffer state
- the next readable publication, if any, must be acquired by a later
  `refresh_readable_count()`

Pointer rules:

- `dst == nullptr` with `count != 0` fails
- `dst == nullptr` with `count == 0` succeeds if the transport is
  initialised

## Readable-count semantics

`current_readable_count()` reports the unread count in the currently
owned consumer buffer only. It does not perform the acceptance
protocol.

`refresh_readable_count()` performs the consumer-side staging protocol
step when no consumer buffer is currently owned and reports the
resulting unread count in the accepted current consumer buffer.

If a consumer buffer is already owned, `refresh_readable_count()`
returns the same value as `current_readable_count()`.

## Status and validity

### `posting_is_ready()`

Shallow producer-side operational readiness.

Intended meaning:

- transport is initialised
- producer is not poisoned

### `reading_is_ready()`

Shallow consumer-side operational readiness.

Intended meaning:

- transport is initialised

### `posting_poisoned()`

Reports terminal producer-side allocation failure state.

### `posting_is_valid()`

Producer-side in-flight validity check.

Safe only:

- on the producer thread
- or while quiescent

Checks producer-visible invariants only.

### `reading_is_valid()`

Consumer-side in-flight validity check.

Safe only:

- on the consumer thread
- or while quiescent

Checks consumer-visible invariants only.

### `validate()`

Full structural validation.

Safe only while quiescent.

In addition to producer-visible and consumer-visible validity, this
checks that the consumer-owned buffer is either null or matches the
current producer locked-buffer role.

## Setup and teardown

`initialise_fixed()` and `initialise_growable()` require a deallocated
instance.

`initialise_fixed(capacity, allow_discard)` configures fixed-capacity
operation with optional producer-side discard.

`initialise_growable(capacity, max_capacity)` configures growable
operation with discard disabled.

`deallocate()` releases all owned storage and restores canonical empty state.

Direct setup and teardown are intended for owner or registry control,
not arbitrary role-side use.

## Canonical empty state

Canonical empty state means:

- not initialised
- not poisoned
- no allocated buffer storage
- zero capacities and sizes
- discard disabled
- default producer and consumer role indices
- default phases
- default staged word value

This state is used for deallocated-state validation.

## Invariants

While initialised, the intended invariants are:

- `m_max_capacity != 0`
- all three buffers are allocated
- each buffer capacity is in the range `[k_min_capacity, m_capacity]`
- each buffer size is in the range `[0, capacity]`
- producer role indices form a permutation of the three buffers
- staging word is in the encoded range `[0, 7]`

While producer-ready, the intended producer-side invariants also
include:

- producer is not poisoned
- `m_capacity <= m_max_capacity`

While consumer-owning a buffer, the intended consumer-side invariants are:

- consumer buffer index is in `[0, 2]`
- consumer read index is in the range `[0, buffer.size]`

While deallocated, the intended invariants are:

- capacities are zero
- buffer storage is null
- sizes are zero
- discard is disabled
- default phases and role indices are restored
- consumer buffer is null
- consumer read index is zero
- staged word equals the default staged value

## Non-goals and caveats

- `TQueue<T>` is not a general-purpose queue abstraction
- phase is part of the acceptance protocol, not a user-facing sequence
  number
- fixed-capacity discard is intentionally coarse
- consumer-side observation does not attempt to reconstruct hidden
  producer history beyond the explicit protocol
- producer-side poisoning is terminal for producer use
- validation strength depends on call context
