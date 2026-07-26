# Current Scope Backlog

Status: active priorities only. Completed-task history is intentionally omitted.

This file is a short navigation layer rather than an independent detailed
backlog:

- `manifold_engine_interim_backlog.md` contains the broader engine backlog and
  subsystem dependencies.
- `../memory/memory_refactor_handoff.md` contains the current memory/container
  architecture and detailed follow-on constraints.
- `../containers/slot_sandwich_refactor_handoff.md` records the completed slot
  sandwich design, retained capabilities, and validation history.
- `../system/erased_owner_handoff.md` defines the active `CErasedOwner`
  replacement and its staged transport integration.

## Current Priorities

1. Complete the strong system-ID and module mounting-point prerequisite
   described by `../system/erased_owner_handoff.md`. The erased-owner hazard,
   attribution, and future provisioning surfaces must not encode the current
   ambiguity between stable mounting points and module variants.
2. Implement the first buildable `CErasedOwner` checkpoint described in
   `../system/erased_owner_handoff.md`: system-owned transported types and
   registration, direct token-backed erased ownership, explicit destruction
   dispatch, hazards, and migration through the existing `TOwning` without
   automatic reattribution.
3. Continue the bounded Rigtorp-style MPMC transport work for the upcoming
   debug infrastructure and later job system. The initial single-header
   transport-family checkpoint is now landed and validated; remaining work is
   naming, stronger contention coverage, wrapper review, and downstream
   integration. The agreed lifecycle states are `open`, `closing`, `closed`,
   and `shutdown`, with outstanding-reservation accounting for race-free
   draining and closure.
4. Continue the low-level text ingester and JSON infrastructure work described
   in the interim backlog, with debug-system slices added where they support
   diagnostics and controlled shutdown.

## Subsequent Cross-Cutting Work

- Complete allocator bootstrap ordering before removing the temporary fallback
  allocator.
- Resume thread provisioning, TLS, and formal threading stress tests when the
  Linux build path makes cross-platform validation practical.
- Build the Windows and Linux platform modules around system pumps, input,
  windows, and dialogs rather than relocating the existing threading wrappers.
