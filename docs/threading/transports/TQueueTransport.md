Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

File:   TQueueTransport.md  
Author: Ritchie Brannan  
Date:   15 Apr 26  

# TQueue

## Overview

`TQueue<T>` is a single-producer, single-consumer transport for trivially copyable typed elements.

It is designed for sequential transport rather than shared random access. The producer appends elements and publishes buffer states through a compact atomic staging word. The consumer reads accepted buffers from the start and never partially adopts a new buffer. Communication is asymmetric:

- the producer owns publication and buffer-role transitions
- the consumer owns read position within its currently accepted buffer
- staged publication is transferred through a single atomic staging word
- phase is used as part of the consumer acceptance protocol

The transport supports two capacity policies:

- fixed-capacity mode, optionally with producer-side discard of pending producer-managed backlog
- growable mode, where producer-managed buffers may grow up to a configured maximum

Allocation failure during producer-side reallocation is treated as a terminal producer-side fault. In that state the producer is poisoned and further `post()` operations fail. Data already published remains valid for consumer-side reading.

## Scope

`TQueue<T>` provides:

- SPSC transport of trivially copyable `T`
- single-element and bulk `post()`
- single-element and bulk `read()`
- all-or-nothing bulk semantics
- producer-side publication through staged buffer exchange
- consumer-side phase-gated acceptance
- optional producer-side discard policy in fixed-capacity mode
- optional producer-side growth in growable mode

`TQueue<T>` does not provide:

- MPSC, SPMC, or MPMC semantics
- blocking or waiting semantics
- general-purpose queue semantics across arbitrary threads
- in-transport lifecycle arbitration between producer and consumer wrappers
- recovery from allocation failure
- preservation of all pending producer-side backlog when discard policy is enabled

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
- output / staged / locked buffer role indices

A single atomic staging word transfers publication state. The staging word encodes:

- zero: no staged publication currently present in the word
- non-zero low bits: published buffer identity
- phase bit: current producer publication phase

The transport also maintains configuration state:

- `m_capacity`: current active producer-side working capacity
- `m_max_capacity`: configured maximum capacity / readiness marker
- `m_allow_discard`: fixed-capacity discard policy enable
- `m_allocation_failed`: terminal producer-side allocation failure state

## Observation model

### Producer-side observations

Producer-side observation is limited to producer-owned state plus the staging word.

The producer may inspect and mutate:

- producer role indices
- producer phase
- producer-managed buffer sizes and capacities
- current active capacity
- staging word by atomic exchange

The producer must not rely on consumer-owned read position or other consumer-local state.

### Consumer-side observations

Consumer-side observation is limited to consumer-owned state plus the staging word.

The consumer may inspect and mutate:

- current consumer buffer index
- current read index
- consumer phase
- staging word by atomic exchange

The consumer does not attempt to reconstruct hidden producer internal history beyond what is represented by the acceptance protocol itself.

### Quiescent observation

Full structural validation requires quiescence. Quiescent validation may inspect:

- all buffers
- all role indices
- all phases
- all capacities and sizes
- staging word state

Quiescent validation is stronger than in-flight role-specific validity checks.

## Capacity model

Configured capacities are treated as minimum requested capacities, not necessarily exact internal capacities.

Internal working capacities may be conditioned by implementation policy, including:

- minimum capacity guarantee
- growth-policy conditioning

In fixed-capacity mode:

- `m_capacity == m_max_capacity` after initialisation
- no producer-side growth occurs
- discard may be enabled

In growable mode:

- the initial working capacity may be lower than the configured maximum
- producer-side posting may increase `m_capacity` up to `m_max_capacity`
- discard is not enabled

The consumer does not own capacity changes. Producer-side growth may leave producer-managed buffers transitioning toward the new capacity over time.

## Publication model

Each producer `post()` appends new elements to the current output buffer and then publishes that buffer through the staging word.

There are two main publication outcomes.

### Previously staged publication not yet consumed

If the producer publishes and the exchange does not return zero:

- the previously staged publication had not yet been consumed
- publication remains in the current producer phase
- producer-side pending backlog is continued
- the old staged buffer is updated to mirror the new producer-side pending state
- output and staged roles are rotated

### Previously staged publication already consumed

If the producer publishes and the exchange returns zero:

- the previously staged publication had already been consumed by the consumer
- the just-published current-phase buffer is treated as obsolete for consumer progression
- the producer flips phase
- the producer republishes a canonical rebased buffer for the new phase
- producer-side role indices are updated accordingly

This is the normal rebase path.

## Consumer acceptance model

The consumer interacts with the staging word only through the normal acceptance protocol.

The consumer maintains an expected phase.

When the consumer needs a new readable buffer, it exchanges the staging word with zero and interprets the returned publication:

- zero: no staged publication available
- matching expected phase: publication accepted, consumer phase toggles
- mismatched phase: no currently acceptable readable buffer is acquired

Clearing the staging word during this exchange is intentional protocol processing, even when the returned phase is not currently acceptable. The staging word is not treated as a passive mailbox to be preserved for later re-reading. It is an active handoff point in the producer / consumer protocol.

Accepted consumer buffers are always read from the start. The consumer never partially adopts a new buffer.

## Discard policy

Discard applies only to producer-managed pending backlog.

Discard does not revoke or mutate:

- consumer-owned current buffer
- elements already accepted by the consumer

When discard is enabled and growth cannot make the new posting fit:

- producer-side pending backlog may be abandoned
- producer-managed pending buffers are reset to begin again from the new posting
- consumer-owned data already acquired remains valid and readable

Discard is intentionally coarse. It is a pressure policy for this transport and does not attempt to preserve a newest suffix subset of pending producer backlog.

Discard also does not imply a separate telemetry-style retention policy. This transport preserves simple consumer semantics rather than maintaining a freshness window.

## Allocation failure and poisoning

Producer-side reallocation failure is treated as a terminal producer-side fault.

When producer-side allocation failure occurs:

- `m_allocation_failed` is set
- further `post()` operations fail
- producer-side readiness and validity fail
- consumer-side already published data remains valid
- consumer may continue draining accepted / staged data according to the normal protocol
- the transport instance should be retired or deallocated by the owner layer

The transport does not attempt to recover from allocation failure.

## Read semantics

`read()` is all-or-nothing.

For a bulk read to succeed:

- the requested count must be available contiguously in the current accepted consumer buffer after any required protocol refresh
- otherwise the read fails and consumer state is not advanced by that read

`current_readable_count()` reports the unread count in the currently owned consumer buffer only.

`refresh_readable_count()` performs the consumer-side staging protocol step if needed and reports the resulting unread count in the accepted current consumer buffer.

## Post semantics

`post()` is all-or-nothing at the API boundary under normal operational refusal.

A posting succeeds only if the entire requested count can be accepted under the active capacity policy.

A `post()` may fail because:

- the transport is not ready
- the producer is poisoned
- the request exceeds configured capacity policy
- fixed-capacity mode cannot accept the request and discard is not allowed

A `post()` may also fail after partial internal progress if a producer-side reallocation fails. In that case:

- the producer becomes poisoned
- previously published data remains valid for the consumer
- the instance is no longer considered healthy for further producer use

This is an intentional terminal-fault policy.

## Status and validity

### `producer_is_ready()`

Shallow producer-side operational readiness.

Intended meaning:

- transport is initialised
- producer is not poisoned

### `consumer_is_ready()`

Shallow consumer-side operational readiness.

Intended meaning:

- transport is initialised

### `producer_poisoned()`

Reports terminal producer-side allocation failure state.

### `producer_is_valid()`

Producer-side in-flight validity check.

Safe only:

- on the producer thread
- or while quiescent

Checks producer-visible invariants only.

### `consumer_is_valid()`

Consumer-side in-flight validity check.

Safe only:

- on the consumer thread
- or while quiescent

Checks consumer-visible invariants only.

### `validate()`

Full structural validation.

Safe only while quiescent.

## Setup and teardown

`initialise_fixed()` and `initialise_growable()` require a deallocated / not-ready instance.

`deallocate()` releases all owned storage and restores canonical empty state.

Direct setup/teardown is intended for owner / registry control, not arbitrary role-side use.

## Canonical empty state

Canonical empty state means:

- not initialised
- not poisoned
- no allocated buffer storage
- zero capacities and sizes
- default producer and consumer role indices
- default phases
- zero staging word

This state is used for deallocated-state validation.

## Non-goals and caveats

- `TQueue<T>` is not a general-purpose queue abstraction
- phase is part of the acceptance protocol, not a user-facing sequence number
- fixed-capacity discard is intentionally coarse
- consumer-side observation does not attempt to reconstruct hidden producer history beyond the explicit protocol
- producer-side poisoning is terminal for producer use
- validation strength depends on call context