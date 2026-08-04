# Current Scope Backlog

Status: active priorities only. Completed-task history is intentionally omitted.

This file is a short navigation layer rather than an independent detailed
backlog:

- `manifold_engine_interim_backlog.md` contains the broader engine backlog and
  subsystem dependencies.
- `../project/future_work_notes.md` retains useful cross-task context that is
  not an active priority.
- `../project/completed_milestones.md` records completed cross-cutting work.
- Permanent architecture belongs to the subsystem documents under `docs/`.

## Current Priorities

1. Continue the low-level text ingester and JSON infrastructure work described
   in the interim backlog, with debug-system slices added where they support
   diagnostics and controlled shutdown.

## Subsequent Cross-Cutting Work

- Update documentation describing AI involvement, contribution, authorship, and
  specification so it reflects the current collaborative workflow.
- Add command-line selection of named tests and test groups without requiring
  the complete aggregate suite.
- Rebrand the public infrastructure to avoid confusion with *Manifold Garden*,
  including repository names and links.
- Complete allocator bootstrap ordering before removing the temporary fallback
  allocator.
- Resume thread provisioning, TLS, and formal threading stress tests when the
  Linux build path makes cross-platform validation practical.
- Build the Windows and Linux platform modules around system pumps, input,
  windows, and dialogs rather than relocating the existing threading wrappers.
