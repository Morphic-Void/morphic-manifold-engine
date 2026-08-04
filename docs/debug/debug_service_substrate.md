Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

File:   debug_service_substrate.md
Author: OpenAI Codex
Date:   28 Jul 2026

# Debug Service Substrate

## Scope

`debug/service.hpp` and `debug/service.cpp` provide the first bounded
implementation checkpoint for the replacement debug system.

The service substrate sits beside the placeholder implementation in
`debug/debug.cpp`. The replacement public macros are included through
`debug/debug.hpp`; existing usage migration remains separate.

This checkpoint establishes:

- executable ownership through `TInstance<CDebugServiceState>`;
- one module-local service pointer with explicit installation and removal;
- cache-isolated shared control words;
- an executable-owned MPMC event transport;
- a dedicated writer thread which services the event log;
- a mutex-guarded synchronous direct log;
- orderly transport close, drain, thread join, and service destruction;
- a narrow provisional host lifecycle integration;
- the replacement statement-only public macro interface;
- independent focused tests.

It does not migrate existing call sites or implement optional sinks, DLL
exports, or generated static usage-point data.

## Ownership

The executable creates one:

```cpp
TInstance<debug_system::CDebugServiceState>
```

The payload owns all state needed by producers and the writer:

- incident, shutdown-request, configuration, wake, and writer-state words;
- the MPMC arena transport;
- direct-log lock, log, and copied path;
- a 4096-byte direct formatting buffer;
- writer thread lifetime;
- event log and copied path;
- a 4096-byte event formatting buffer;
- the event transport.

The payload is immovable after provisioning. Its owner must not be reset,
re-emplaced, moved, or reattributed until producers have quiesced, the writer
has stopped, and every module-local pointer has been removed.

The host normally configures and explicitly opens both logs before publishing
the service pointer. The writer thread exclusively writes, services, flushes,
and closes the event log after startup. Any thread may synchronously enter the
separately locked direct path while the service remains live.

Opening is not required to be eager. Both paths remain stored when opening is
deferred. Writer startup ensures that the event log is open, and the first
direct write opens the direct log while holding its lock.

Operations on service-owned state are members of `CDebugServiceState`.
Module-pointer provisioning and the global reporting facade remain free
functions because they act on module-local routing rather than an already
selected service object.

## Provisioning

Each linked module contains one pointer in its own `service.cpp`:

```cpp
CDebugServiceState* s_service;
```

The module-local interface is:

```cpp
get_service()
install_service(service)
uninstall_service(expected)
```

Installation is not concurrently replaceable. It occurs before the module
exposes reporting work. Removal occurs after that work and its threads have
quiesced. The pointer is intentionally non-atomic because concurrent
installation, replacement, or removal is outside the lifetime contract.

DLL export names, ABI versioning, executable callback boundaries, and combined
installation of debug, identity, and memory provisioning remain deferred until
module loading is implemented.

## Shared Words

Every independently shared service word uses
`TCacheLineAtomic<std::uint32_t>`:

```text
incident_counter
shutdown_request
configuration
writer_wake_epoch
writer_state
```

This rule does not apply to per-event transport sequence words or future
macro-local breakpoint overrides.

### Incident counter

Allocation uses an atomic increment and ordinary `std::uint32_t` wrapping
semantics. Identities progress from 1 through `UINT32_MAX`, then 0, then 1
again. Zero is therefore valid after wrap. A discontinuity in a log containing
enough events to wrap the counter is sufficient for later ordering
reconstruction.

Identity allocation uses relaxed ordering because uniqueness does not publish
event content.

### Shutdown request

The word contains an `EShutdownReason`. Zero means no request.

Multiple reporting threads may request shutdown. Reasons are ordered by
severity and updates are monotonic: a weaker request cannot erase or downgrade
a stronger one. The host reads the request and owns coordinated application
shutdown. The word does not represent shutdown progress and is not cleared.

The current ordered reasons are `critical_incident`, `fatal_incident`, and
`panic_incident`. The existence of a reason does not by itself require every
incident of that severity to request shutdown; that remains reporting policy.

### Configuration

The host thread is the sole writer. It publishes complete word snapshots;
client and writer threads perform read-only relaxed loads. The initial defined
bit is `k_breakpoints_enabled`.

Configuration bits are currently self-contained policy. Publishing associated
out-of-word data would require an explicit synchronization or immutable
snapshot design.

## Event Transport

The transitional event contains:

```text
uint32 incident identity
uint32 content size
uint32 content type
uint8 event level
uint8 event type
uint16 literal prefix size
64-bit ambient system identity
uint32 source line
uint32 source suffix size
96-byte source suffix buffer
192-byte content buffer
72-byte bounded argument descriptor
120-byte expansion reserve
```

`SEvent` is fixed at 512 bytes with a compile-time size assertion. The trailing
120-byte reserve allows modest metadata and routing expansion without changing
the transport memory footprint or slot layout. Existing field capacities are
not enlarged merely to consume the reserve.

The level, type, and literal-prefix size completely occupy the four bytes which
would otherwise be alignment padding before `SEventSource`. Compile-time offset
assertions fix level at byte 12, type at byte 13, literal-prefix size at byte
14, source at byte 16, content at byte 128, arguments at byte 320, and expansion
at byte 392.

The literal-prefix size is zero for ordinary formatted events. Assertion events
use it to mark the stringized condition and fixed explanatory prefix as literal
text; only the optional message suffix is interpreted as a format. This keeps
legal C++ braces in condition expressions from acquiring format semantics and
requires no producer-side parsing or escaping.

Typed submission receives level and type as compile-time template parameters.
The permitted compositions are:

```text
info/detail/trace  + event
assert             + condition
warning/error      + event
critical/fatal     + condition or event
```

Invalid compile-time combinations are rejected by `static_assert`. The writer
also validates transported metadata before reconstruction.

Typed submission captures the producing thread's ambient
`system_ids::id_type`, the compiler-provided source line, and a static source
file literal. The source literal length is known by the call-site template, so
capture does not scan it. A source path requiring 96 bytes or more is truncated
from the left: the rightmost 95 bytes are retained and followed by a
terminator. No source hash or truncation marker is transported.

Missing or invalid ambient system identity rejects normal submission. This
enforces the surrounding startup, thread, DLL-binding, and teardown contract;
the later bounded infrastructure-failure route remains responsible for
diagnosing a violation without recursion.

The argument descriptor is stable within this design checkpoint:

```text
uint32 packed type tags
uint8  parameter count
uint8  payload size
uint16 reserved
64-byte payload
```

There are at most eight parameters. Each type tag occupies four explicitly
shifted bits; C++ bitfield layout is not used. Values are packed consecutively
with `memcpy`, so the payload contains no alignment padding. Typed submission
encodes directly into a reserved arena slot. If no slot is available, the
producer encodes a temporary descriptor and formats it while holding the
direct-log lock.

The implemented argument tags are:

```text
unused
false
true
int32
uint32
int64
uint64
float32
float64
inline_text
external_string_reference
external_path_reference
```

Boolean values consume no payload bytes. `CInlineText16` owns exactly 16 bytes,
including a guaranteed terminator, and therefore carries at most 15 text
characters. External string and path references reserve stable tag values and
eight payload bytes but deliberately have no public wrappers or resolvers yet.
Four tag values remain unassigned.

Unsupported C++ argument types, more than eight arguments, and a payload over
64 bytes are compile-time errors. Pointers, references as transported state,
arbitrary strings, and implicit object conversions are not admitted.

The initial format grammar supports sequential `{}` substitutions plus `{{`
and `}}` escaped braces. Argument types determine their default textual
representation. There are no formatting modifiers in this checkpoint.
Malformed formats, descriptor inconsistencies, unsupported reserved reference
types, and formatting-buffer exhaustion are explicit formatter results.

The transport retains a provisional capacity hint of 128, giving its typed
arena a fixed 64 KiB footprint. The copied format and source capacities remain
provisional within the 512-byte element. Subject to use of the explicit
expansion reserve for later additions, the normal transport payload now
contains every field required by the current design.

The free `submit_text()` facade resolves the module-local service and delegates
to `CDebugServiceState::submit_text()`. It remains as a compatibility facade
and submits short text as uninterpreted content, so braces in existing text do
not acquire format semantics. Typed `submit_event()`:

1. allocates an incident identity;
2. reserves an MPMC event slot;
3. copies the bounded format and encodes arguments directly into that slot;
4. publishes the completed event and signals the writer wake epoch;
5. formats and writes synchronously under the direct-log lock when no slot is
   immediately available.

Breakpoint- or shutdown-capable macros use `process_event()`. It captures one
incident context, submits that context through the same transport/direct path,
applies any shutdown request independently of submission success, and finally
applies breakpoint policy using that same context. The macro-local breakpoint
override is passed by reference so debugger controls operating on the producing
thread can change the usage-point state without cross-thread synchronisation.

The host-controlled configuration word currently assigns bit 0 to global
breakpoint enable, bits 1-2 to the runtime informational ceiling, and bit 3 to
critical-incident shutdown escalation. Fatal and panic consequences do not
depend on the critical escalation bit.

The writer validates the descriptor, interprets the format, and constructs the
final text in its service-owned 4096-byte buffer. Invalid transported events
are represented by a fixed diagnostic in the direct log rather than trusting
the malformed event further.

The current record envelope renders the transported system identity as a
fixed-width hexadecimal value, renders the compositional level and type, and
adds the source suffix and line when they are available:

```text
[decimal incident identity] [hex system identity] [level:type] [source suffix:line] text
```

The direct formatting buffer is covered by the same mutex as direct-log
opening, writing, and flushing. This makes concurrent producer fallback safe
without stack-resident 4096-byte buffers. Event and direct routes use the same
record reconstruction.

## Writer Lifetime

The native entry function validates and casts its opaque pointer, then invokes
`writer_thread_main()` through a service reference. The main function reads as
the sequential writer-thread program: it ensures that the event log is open,
publishes `running`, and then alternates between draining and parking. In the
preferred host lifecycle the log is already open; this check preserves
deferred opening.

The wake protocol loads an epoch before draining. A producer increments that
epoch after publication and wakes one waiter. If publication races the writer
between drain completion and parking, the changed epoch prevents a lost wake.

Orderly shutdown is:

```text
producers quiesce
transport open -> closing
writer wake
writer drains and recycles every outstanding event
transport closing -> closed
writer flushes and closes event log
writer publishes stopped
host joins writer
host closes direct log
module pointers are removed
TInstance is destroyed
```

Forced transport shutdown and recovery from a writer which cannot complete are
not part of this orderly checkpoint.

## Host Integration

`host.cpp` is a thin provisional lifecycle client. It:

- creates the service and configures both stored log paths;
- explicitly opens both logs before installing the service pointer;
- installs the service and starts the writer before provisional worker startup;
- checks the shutdown request in the existing host iteration;
- stops the service after worker shutdown;
- removes the executable-module pointer before owner destruction.

The current TGA processing and worker-management sketch did not influence the
service internals. Future backing-file, multi-step-operation, module-loading,
identity, and memory-provisioning infrastructure should relocate these
lifecycle calls rather than redesign the service.

## Validation

`DebugService_test_suite.cpp` independently covers:

- owner creation;
- installation, duplicate rejection, expected removal, and accessor state;
- incident allocation;
- complete configuration publication;
- monotonic shutdown reasons;
- compile-time supported-type classification;
- fixed 72-byte descriptor shape;
- tag ordering and zero-payload boolean encoding;
- the eight-argument and 64-byte payload boundaries;
- integer, floating-point, boolean, and inline-text formatting;
- sequential substitutions and escaped braces;
- malformed format, mismatched argument, invalid descriptor, unsupported
  reserved-reference, and output-capacity results;
- writer startup and state publication;
- MPMC legacy-text and typed-event delivery;
- ambient system identity capture on the producer;
- source-line capture and deterministic left truncation of long source paths;
- raw system identity and source metadata reconstruction;
- compile-time and writer-side level/type validation;
- explicit packed metadata offsets and bounded literal-prefix validation;
- identical level/type reconstruction on event and direct routes;
- explicit pre-install opening of both configured log paths;
- deferred event-log opening by writer startup;
- deferred direct-log opening on first direct write;
- oversized direct fallback;
- orderly drain, close, and join;
- event and direct file output.

The focused suite reports 85 passing checks on both x64 and x86.

Implementation of the direct fallback exposed and corrected an existing
`platform::filesystem::Log::flush()` result inversion: `std::fflush()` returns
zero on success.

## Deferred Design

The following remain deliberately unsettled:

- final queue-capacity policy;
- any justified extension of the deliberately minimal format grammar;
- external string and path reference wrappers, strong typing, and lookup;
- eventual migration from `char` storage to the codebase's enforced UTF-8
  representation based on `std::uint8_t`;
- `OutputDebugString` and internal terminal or popup sinks;
- direct-path recursion handling and lowest-level platform fallback;
- the intentional reporting gap for `platform::threading::CThread` and
  `platform::threading::CExclusiveLock`, which remain outside the normal debug
  transport because the service itself depends on them;
- DLL ABI/version validation and executable-hosted direct call boundary;
- final shutdown-reason catalogue and host policy;
- migration of existing usages and retirement of the placeholder utilities.

When the main debug implementation and usage migration are complete, remind
the project owner to perform a manual formatting pass before treating the
subsystem as finished.
