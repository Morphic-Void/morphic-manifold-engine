# Current Scope Backlog

Status: active priorities only. Completed-task history is intentionally omitted.

This file is a short navigation layer rather than an independent detailed
backlog:

- `manifold_engine_interim_backlog.md` contains the broader engine backlog and
  subsystem dependencies. Its legacy filename is retained temporarily and is
  included in the deferred Morphic rebranding audit.
- `../project/future_work_notes.md` retains useful cross-task context that is
  not an active priority.
- `../project/completed_milestones.md` records completed cross-cutting work.
- Permanent architecture belongs to the subsystem documents under `docs/`.

## Current Priorities

1. Continue the low-level text ingester and JSON infrastructure work described
   in the interim backlog, with debug-system slices added where they support
   diagnostics and controlled shutdown.

## Subsequent Cross-Cutting Work

- Add command-line selection of named tests and test groups without requiring
  the complete aggregate suite.
- After the current layout and consolidation work, add a tool-independent
  build description that preserves Core as per-consumer shared source, attempt
  Linux compilation, and distinguish portable-Core issues from missing Linux
  platform implementations.
- After that build-portability pass, audit and complete the
  Manifold-to-Morphic rebrand, including the legacy backlog filename, local
  working directory, GitHub repository, references, and links.
- Build a pre-build code policy validator once the new layout and component
  rules are stable, covering approved allocation paths, STL and exception
  restrictions, ID registration, includes, and dependency direction.
- Complete allocator bootstrap ordering before removing the temporary fallback
  allocator.
- Resume thread provisioning, TLS, and formal threading stress tests when the
  Linux build path makes cross-platform validation practical.
- Build the Windows and Linux platform modules around system pumps, input,
  windows, and dialogs rather than relocating the existing threading wrappers.
