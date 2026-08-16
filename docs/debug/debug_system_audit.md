Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

File:   debug_system_audit.md
Primary draft: OpenAI tools
Reviewed and accepted by: Ritchie Brannan
Date:   26 Jul 2026

# Debug System Audit

## Purpose

This document records the pre-redesign state of the Morphic Engine runtime
debug surface and its call sites. It is an audit, not the contract for the
replacement system.

The audit covers:

- `core/debug/debug.hpp`;
- `core/debug/debug.cpp`;
- all runtime macro and `debug_utils` call sites under the peer ownership directories;
- current build configuration and focused test interaction;
- the existing MPMC transport, platform log, system identity, and threading
  surfaces that constrain the replacement.

Compile-time `static_assert` use is out of scope. The independent ordered- and
unordered-slot test harness trap implementations are also out of scope because
they are test-framework mechanisms rather than users of the runtime debug API.

## Current public surface

The public surface consists of:

- `MV_HARD_ASSERT(expression)`;
- `MV_SELF_ASSERT(expression)`;
- `MV_FAIL_SAFE_ASSERT(expression)`;
- `MV_CRITICAL_ASSERT(expression)`;
- `debug_utils::enable_asserts(bool)`;
- `debug_utils::disable_asserts()`;
- `debug_utils::hard_fail()`;
- `debug_utils::fail_safe(bool)`;
- `debug_utils::debug_output(const char*, ...)`.

`MV_DEBUG_BUILD` defaults to enabled when `_DEBUG` is defined or `NDEBUG` is
not defined. The MSBuild Debug configurations define `_DEBUG`; Release
configurations define `NDEBUG`. The definition can be overridden externally.

### Evaluation and reaction semantics

| Surface | Debug evaluation | Release evaluation | Current reaction |
|---|---|---|---|
| `MV_HARD_ASSERT(x)` | evaluates `x` once | does not evaluate `x` | traps when false and assertions are enabled |
| `MV_SELF_ASSERT(x)` | evaluates `x` once | does not evaluate `x` | none of its own |
| `MV_FAIL_SAFE_ASSERT(x)` | evaluates `x` once | evaluates `x` once | traps when false in Debug, then returns the Boolean result |
| `MV_CRITICAL_ASSERT(x)` | same as hard assert | same as hard assert | aliases `MV_HARD_ASSERT` |
| `debug_output(...)` | active | active | formats and sends text to the Windows debugger |

These are evaluation contracts as well as notification choices. Migration
cannot safely map the four names to four severities without reviewing each call
site.

`hard_fail()` is not a terminating function. It returns when assertions are
disabled and may also return when execution is resumed after a debugger trap.
Callers therefore contain their own continuation, fallback, or return behavior.

`MV_SELF_ASSERT` does not inspect the result of its expression. Its two current
expressions perform integrity checks which report failures internally, making
the macro an expensive-debug-work gate rather than a conventional assertion.

## Inventory

There are 162 runtime assertion macro uses outside the macro definitions:

| Surface | Uses |
|---|---:|
| `MV_HARD_ASSERT` | 72 |
| `MV_FAIL_SAFE_ASSERT` | 76 |
| `MV_CRITICAL_ASSERT` | 12 |
| `MV_SELF_ASSERT` | 2 |

There are also:

- 31 `debug_utils::debug_output` calls;
- four suites that disable and re-enable assertions;
- no focused runtime debug-system tests.

Assertion uses by subsystem are:

| Area | Uses |
|---|---:|
| containers | 63 |
| threading | 42 |
| platform | 21 |
| memory | 19 |
| system | 13 |
| host prototype | 3 |
| image codec | 1 |

Eighty-eight assertion uses are in public headers and 74 are in source files.
Header usage means a static usage identity should describe the source usage
point, not produce a different identity for each template instantiation.

## Usage classes

### Fail-safe checks

`MV_FAIL_SAFE_ASSERT` is predominantly a report-and-continue Boolean operation.
It is used:

- directly as an `if` condition;
- negated as a guard condition;
- as a returned Boolean result;
- as a standalone check whose result is intentionally discarded;
- inside short-circuit expressions that combine validation and state changes.

These uses occur in allocation and attribution accounting, native lock and
thread lifetime code, parking primitives, queue transports, host thread entry
points, string integrity checks, and image allocation.

The expression must continue to execute in all builds. Many callers use its
result to select a safe return path. Some expressions themselves perform
required work, including allocator callbacks and accounting changes.

### Hard checks

Most `MV_HARD_ASSERT` calls are debug-only precondition, postcondition, and
invariant checks. Common patterns include:

- constructor and destructor state validation;
- checked access followed by a fail-safe sentinel return;
- post-allocation and post-mutation invariant validation;
- unreachable or corrupt-state markers followed by local fallback behavior.

Execution is allowed to continue after the trap. Several `false` sites return a
sentinel immediately afterwards, while POD vector accessors return a shared
last-gasp object.

One call requires special migration attention:

```cpp
MV_HARD_ASSERT(to.sub(allocation_count, bytes));
```

This is the rollback after a failed attribution transfer in
`memory::reattribute`. The rollback expression is omitted in Release by the
current macro, despite being required state-changing work. The replacement
should separate the rollback operation from reporting rather than preserve this
accidental build-dependent behavior.

### Critical checks

The 12 critical sites mark closed-world type dispatch, ownership transport, and
attribution-accounting failures:

- seven in erased-owner lifetime and payload dispatch;
- four in erased-owner transport reattribution/posting paths;
- one for an unexpected owning payload in the host prototype.

These sites generally already provide degraded local continuation: return a
failure or zero value, skip unknown destruction, deliver an unexpectedly
reattributed item, or continue after restoring ownership. The future critical
policy must preserve the distinction between recording/escalation and the
caller's local control flow.

### Self checks

Both self checks wrap `CStableStrings::check_integrity()` around sorting. They
are debug-only potentially expensive validation. The integrity implementation
reports through hard or fail-safe paths when invalid.

This category should remain an explicit debug-only validation facility, but it
does not need to masquerade as a distinct incident severity.

### Debug output

All 31 `debug_output` calls are in `host/runtime/host.cpp`, which identifies itself
as a test/sketch/prototype. They describe:

- thread startup, waiting, waking, and exit;
- heartbeat and message traffic;
- file and TGA prototype operations;
- recognized and unrecognized transported payloads.

Every current format argument is a string literal. Dynamic arguments are
limited to thread-name C strings, epochs, message/type identifiers, and an
unrecognized-message identifier. Callers include their own newline.

These uses are trace/informational events rather than assertion failures. Some
high-frequency sites, notably heartbeat and OS-pump output, will require
quieting or category policy rather than assertion suppression.

## Placeholder implementation behavior

### Global assertion switch

Assertion reaction is controlled by one process-global atomic Boolean.
`enable_asserts()` exchanges the value and returns the previous value.
`disable_asserts()` forwards to it.

Four negative-path test suites suppress assertions:

- erased-owner allocation rejection suppresses one operation;
- queue, ring, and owning-transport suites suppress assertions for the entire
  suite.

The tests restore `true`, not the previous state. There is no scoped guard,
nesting model, per-site quieting, category policy, or thread-local suppression.

### Trapping

The implementation uses:

- `__debugbreak()` with MSVC;
- `__builtin_trap()` with Clang or GCC;
- `std::abort()` as the fallback.

There is no debugger-presence query, breakpoint ownership, cooperative pause,
shutdown escalation, recursion handling, or distinction between development
and published-build policy.

### Text output

`debug_output` uses a 1024-byte stack buffer. It prefixes a process-global
32-bit ordinal, formats with `snprintf` and `vsnprintf`, and calls
`OutputDebugStringA` on Windows.

Current limitations are:

- assertion incidents produce no text, expression, source location, category,
  system identity, native thread identity, or timestamp;
- the ordinal applies only to `debug_output`, not assertions;
- output is silently discarded on non-Windows platforms;
- a null format pointer is not rejected before `vsnprintf`;
- a non-negative `vsnprintf` result is treated as success even when truncation
  occurred;
- format failure emits a fixed line without the reserved ordinal;
- the 32-bit ordinal wraps without explicit handling;
- there is no file output, routing policy, sink failure state, or durability
  contract.

## Integration constraints

### Bootstrap and reentrancy

The current trap path allocates no memory and is callable from memory
allocation, deallocation, accounting, lock, thread-lifetime, and transport
code. The replacement must retain a reporting path that does not depend on the
subsystem reporting the failure.

In particular, reporting from:

- memory code cannot require successful dynamic allocation;
- exclusive-lock code cannot require the same lock implementation;
- thread startup or teardown cannot require fully installed thread context;
- transport code cannot require the failed transport operation to succeed.

Recursive reporting and reporting before full debug-system startup or after
shutdown begins must have defined bounded behavior.

### MPMC transport

`TMpmcArenaTransport` provides the required bounded, non-allocating MPMC
reserve/populate/publish and acquire/process/recycle path.

Its relevant contracts are:

- reserve failure is a normal immediate admission failure;
- successful reserve creates an irrevocable publish obligation during orderly
  operation;
- successful acquire creates an irrevocable recycle obligation;
- closing rejects new reservations but permits draining;
- forced shutdown may abandon outstanding obligations;
- each arena slot contains one persistent live payload object.

The debug producer must therefore fully define how a reserved record is made
publishable even if transitional formatting fails. Queue-full, unavailable,
oversized, recursive, and degraded-system cases require the separate fallback
lane already anticipated by the design.

### File output

`platform::filesystem::Log` is a move-only `FILE*` wrapper with formatted write,
stdio flush, durable flush, and close operations. It has no synchronization,
record framing, error state beyond immediate return values, or emergency
semantics.

It may be useful below the normal or emergency writer, but it is not currently
a thread-safe debug sink.

### Identity

The engine has:

- strong static module, thread, mount-point, and combined system identities;
- thread-local ambient engine thread identity;
- a best-effort native OS thread ID query.

The host thread installs executable/host ambient identity. Prototype worker
startup currently reads its configured combined system identity into a local
variable but does not install the corresponding module and thread identities
in ambient context. Debug capture must tolerate missing identity, and thread
startup integration must eventually install it.

### Build and test structure

Debug and Release are separate Win32 and x64 configurations. The debug system
is compiled into the single current executive project. There is no dedicated
debug-system suite or build-time generation project.

The main executable runs the host prototype before the test suites. This
coupling will matter when tests need isolated debug-system startup, shutdown,
sink substitution, deterministic incident IDs, or controlled failure policy.

## Migration hazards

The migration should account for:

1. Expressions intentionally omitted outside Debug must not acquire accidental
   side effects when moved into a new macro shape.
2. Fail-safe expressions must remain single-evaluation Boolean expressions in
   every build.
3. Hard and critical reports must not steal local continuation policy from the
   caller unless a site is deliberately redesigned.
4. Breakpoints intended for source-level diagnosis must occur on the producing
   thread after the incident context is published.
5. Expensive self-validation needs an explicit compile-time gate.
6. Public-header usage must produce stable source usage identities across
   template instantiations and translation units.
7. Negative tests need bounded suppression or sink/policy substitution instead
   of a process-global unscoped switch.
8. Assertion and logging calls inside low-level memory and threading code need
   a bounded fallback that avoids allocation and dependency recursion.
9. Existing host trace messages are extraction-friendly literals, but the
   interface must distinguish static descriptive text from dynamic payload.
10. Missing ambient engine identity must be representable rather than treated
    as a reporting failure.

## Inputs for public-interface design

The audit establishes the need for caller-facing operations covering these
semantics:

- a debug-only contract check whose expression may be omitted entirely;
- an always-evaluated Boolean check that reports failure and returns the
  original result;
- an architectural/accounting incident with stronger default policy but local
  degraded continuation;
- a debug-only expensive validation gate;
- informational and diagnostic events with static text plus dynamic values;
- explicit shutdown escalation independent of successful log delivery;
- breakpoint policy evaluated on the producer thread;
- scoped quieting or policy substitution suitable for negative tests;
- stable, extraction-friendly static usage identity.

This list does not prescribe macro names, event fields, severities, or routing
policy. Those belong to the interface and technical-design stages.

## Baseline validation

At the end of the audit:

- Debug x64 builds with zero warnings and zero errors;
- Debug x86 builds with zero warnings and zero errors;
- an x64 Debug execution reported all named suites through
  `TMpmcTransport` passing before entering the long exhaustive ordered-slot
  harness;
- the bounded 60-second execution ended during that exhaustive harness, so it
  was not a complete test run.

No runtime source or build configuration was changed by this audit.
