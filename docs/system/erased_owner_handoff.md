# Erased Owner Handoff

Status: carrier design settled, with implementation paused behind the strong
system-ID prerequisite surfaced by the hazard and mounting-point model. The
source remains authoritative once implementation begins.

## Objective

Replace the current `memory::CTypeless` virtual erased-node mechanism with
`CErasedOwner`, a closed-world system owner for one registered payload.

`CErasedOwner` is distinct from `TTypelessPod`:

- `TTypelessPod` stores bounded trivially copyable message data inline.
- `CErasedOwner` owns one separately allocated registered payload through a
  `CMemoryToken`.
- A `CErasedOwner` payload may be POD, but POD is not a requirement or the
  principal use case.

The completed container and token work is a foundation for this change. It
must not be reopened or normalized as part of the erased-owner migration.

## Settled Physical Shape

The intended minimal state is structurally:

```cpp
memory::CMemoryToken m_storage;
strong_type_id       m_type_id;    //  32-bit underlying representation
std::uint32_t        m_hazards;
```

The current 24-byte x64 token makes the intended x64 owner size 32 bytes.
Layout assertions should be added when implementation fixes the final member
order and alignment.

The token remains outside the allocation it may release. Destruction ends the
concrete payload lifetime before token storage is deallocated.

## Ownership And Concurrency

`CErasedOwner` has singular nominal ownership. Only the nominal owner may
mutate the payload, attribution, or hazards. Payload access rights do not imply
ownership or concurrent mutation authority.

The owner contains no atomics. Transport and access structures provide
cross-thread synchronization and ownership-transfer ordering.

The owner is default constructible, move-only, nothrow movable, and nothrow
destructible so that it can be carried by `TOwning`.

Registered payload types must be nothrow default constructible, nothrow move
constructible, nothrow move assignable, and nothrow destructible. Creation
default-constructs the payload in direct token storage. Moving the owner moves
the token and metadata without relocating the payload.

## Type Registration And Dispatch

System transported types, encoded type IDs, and type-to-ID registration belong
under `include/system/`. The current prototype request/result types and their
registrations in `src/host/host.cpp` should move there even though some types
may change as the host design matures.

Registration binds a C++ type to its encoded ID. It is not a mutable runtime
registry.

The shared `TTypeId`, `k_type_id_v`, and registration macro currently in
`types/typeless_traits.hpp` move into the system-owned registration surface.
`TTypelessPod` continues to use them from their new system location. Keeping
the POD mechanism under `types/` does not make system type identity a generic
types-layer responsibility.

Typed load and unload/access operations know the requested C++ type and check
its registered ID against the owner's stored ID. They do not need reverse
dispatch.

Only erased destruction and erased reattribution require ID-to-type dispatch.
Keep these as small explicit switches in `CErasedOwner`. Repeating the short
case list is preferred to introducing generic visitation, descriptor tables,
function-pointer tables, or operation hierarchies solely to centralize it.

The first checkpoint should use a narrow compilation island:

- a system transported-types header containing the current fourteen host
  prototype request/result declarations and registrations;
- an erased-owner header containing the owner declaration and typed templates;
- an erased-owner source file containing the concrete destruction switch and
  including the transported-types header.

This keeps the complete closed payload set out of ordinary owner consumers.
Only the three current `Owning*` result types participate in erased-owner
destruction dispatch; the other registrations remain POD-message types.

Destruction is mechanically uniform after the switch identifies the concrete
type. Reattribution is explicitly customizable per registered owner payload:

- payloads with no nested ownership perform no nested work;
- direct container payloads may delegate to container reattribution;
- structures containing owners may require coordinated type-specific logic;
- types that cannot safely reattribute report that limitation.

Do not infer reattribution support merely from the incidental presence of a
similarly named member function.

## Hazard Contract

Hazards are a 32-bit bitset indexed by stable module bind points, not by the
module variant currently occupying a bind point. Encoded bind-point IDs map to
zero-based linear bit positions in the range `0..31`.

The current source has module-variant IDs but no separate stable mounting-point
or bind-point ID domain. Do not implement hazards by decoding those variant IDs
directly. Adding another packed field to the current shared encoding conflicts
with the schema's intent. The strong ID model and mounting-point semantics are
therefore a prerequisite checkpoint rather than later cleanup.

The intended surface includes:

```cpp
void add_hazard(bind_point_id) noexcept;
void remove_hazard(bind_point_id) noexcept;
bool has_hazard(bind_point_id) const noexcept;
bool has_any_hazard() const noexcept;
std::uint32_t hazard_mask() const noexcept;
```

Hazards normally accumulate as work moves through the system. Removal is an
explicit owner-authorized safety transition after all dependencies on that
bind point have been relinquished. Movement preserves the complete mask.
Reattribution does not alter hazards. No public whole-mask setter is intended.

Future provisioning/context metadata may also use bind-point-indexed fields,
but it is semantically distinct from hazards and is not part of this
checkpoint.

## Virtual Dispatch Boundary

Virtual interfaces are not generally prohibited in TheManifoldEngine.

The relevant hazard is executable identity crossing a module-lifetime
boundary. A vptr, callback, deleter, function pointer, or operation descriptor
stored in transported or externally retained data may become invalid when its
defining DLL is replaced or unloaded.

Virtual interfaces remain valid internal implementation mechanisms when the
interface object and its vtable cannot escape or outlive the defining binary.
Performance and indirection costs remain ordinary local design considerations.

`CErasedOwner` stores no virtual interface or executable operation pointer.
Its recipient resolves registered payload operations locally from the stored
type ID.

## Transport Integration Sequence

The first buildable checkpoint uses the existing
`TOwning<CErasedOwner>` without automatic reattribution. This isolates and
validates owner creation, typed access, movement, destruction, hazards, and
host migration.

A later carrier-specific wrapper will own attribution at transport boundaries:

```text
posting owner -> transport owner -> configured recipient owner
```

The wrapper will have a fixed transport-owner context and an optional fixed
recipient context. Their allocator compatibility is validated before
operation. Posting reattributes to the transport owner. Normal reading
reattributes to the configured recipient when needed; a separate
`read_and_reattribute()` operation is not intended.

The underlying transport remains non-reattributable. Current allocation-owning
transports expose fixed read-only attribution and optional explicit
construction/configuration contexts.

An unexpected read-time reattribution failure is an accounting or corruption
boundary, not a routine incompatibility. The item is still delivered rather
than discarded. A future `MV_CRITICAL_ASSERT` site should record this
development-hard-stop but potentially survivable published-build condition.

## Debug Breadcrumb

The planned `MV_CRITICAL_ASSERT` macro initially aliases `MV_HARD_ASSERT`.
Its definition should document:

- an architectural or accounting contract has failed;
- there is no ordinary local recovery path;
- development should stop;
- a published build may report the fault and continue in a degraded state.

The next debug-infrastructure task will replace the alias with structured
critical reporting. Call-site comments should identify the concrete violated
invariant.

## First Buildable Checkpoint

1. Complete the strong system-ID prerequisite needed to distinguish type,
   module variant, mounting point, thread, and combined system identities.
2. Move the current host prototype transported types, IDs, and type
   registrations into a coherent system-owned header island.
3. Add `CErasedOwner` with direct token storage, registered creation, typed
   access, move-only lifetime, explicit destruction dispatch, and hazards.
4. Replace `memory::CTypeless`, `TTypeless`, and the owning typeless convenience
   surface.
5. Migrate `src/host/host.cpp` to `TOwning<CErasedOwner>` without transport
   reattribution.
6. Replace the unrestricted typeless tests with closed-registration owner
   coverage, including destruction, alignment, movement, allocation failure,
   canonical empty state, invalid typed access, and hazard behavior.
7. Update project files and memory/system documentation.
8. Validate Debug x64 and x86 builds and the focused erased-owner tests.

Do not add the carrier-specific transport wrapper, payload reattribution, or
strong ID types to this first checkpoint.

## Functional Fences

- No virtual interface, callback, deleter, or executable operation pointer is
  stored in the owner or its allocation.
- Only registered system payload types may be loaded.
- Unknown stored IDs are critical invariant failures.
- Payload destruction always precedes storage deallocation.
- Payload addresses remain stable across owner moves and `TOwning` movement.
- Allocation failure leaves a canonical empty owner.
- Owner moves leave the source fully canonical rather than retaining an empty
  configured token.
- `TTypelessPod` remains a separate inline POD-message mechanism.
- Existing public container reattribution and transport attribution behavior
  is not altered by the first checkpoint.

## Later Work

- Add explicit per-type payload reattribution and owner-level reattribution.
- Add the `CErasedOwner`-specific `TOwning` wrapper.
- Add configured producer/transport/recipient attribution validation.
- Integrate `MV_CRITICAL_ASSERT` with the expanded debug infrastructure.
- Add future provisioning/context metadata separately from the hazard field.
