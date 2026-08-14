Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

# System And Local Identity

## Categories

System type identities are the deliberate cross-component identity category.
Any C++ type used by a shared ABI, function query, erased payload, owner,
message, transport, or another component boundary requires a host-authored
system ID. Component-confined erased storage may instead use local identity.
`system/system_type_ids.def` is the sole canonical system definition list.
`MV_REGISTER_SYSTEM_TYPE` binds a C++ type to an existing system ID; it does
not author an ID or a name.

Local type identities belong to one binary component. They may identify a
structure, class, enum, or tag type, but they cannot cross a module boundary.
Copying a validated local short name into a later diagnostic event will be
serialization of text, not export of the identity.

A shared registration-category trait prevents one C++ type from receiving
both system and local registrations in a component.

## Encoding

Both strong ID types occupy 32 bits and use the existing alternating-bit
misuse detector. Fifteen even-position bits encode the ordinal payload:

```text
encoded payload mask  0x15555555
system type flag      0x40000000
allowed-bit mask      0x55555555
invalid-bit mask      0xaaaaaaaa
ordinal mask          0x00007fff
valid ordinals        0..0x7ffe
logical category bit  0x00008000
local tagged indices  0x0000..0x7ffe
system tagged indices 0x8000..0xfffe
invalid tagged index  0xffff
capacity              32767 per category
```

The encoded payload is `spread_to_even_bits(0x7fff - ordinal)`, so it is
always nonzero. A system ID sets the system flag; a local ID leaves it clear.
Consequently a missing category bit cannot accidentally produce a system
identity. Zero remains undefined for both strong types.

Definition lists supply ordinary ordinals. Category-specific `encode_index`
helpers apply the logical category bit, and `decode_index` removes it only
after validating the category. `encode_id` accepts the resulting tagged index;
`decode_id` returns it. This makes the former `0xfffe`/`0xffff` edge explicit:
`0xfffe` is the last valid system index and `0xffff` is invalid.

`system_type_id` and `local_type_id` are distinct strong C++ types. Their
validators also reject raw encodings from the other category.

`type_id` is a third four-byte strong type which preserves either category.
Its constructors from `system_type_id` and `local_type_id` are explicit and
canonicalise invalid or category-mismatched strong values to undefined.
`try_system_type_id()` and `try_local_type_id()` explicitly extract a category
and clear their output on mismatch. No implicit conversion exists among the
three strong types. All three have identical four-byte size and alignment on
x64 and x86, while remaining distinct C++ types.

`TSystemTypeId<T>` and `k_system_type_id_v<T>` expose an explicitly SYSTEM
binding. `TLocalTypeId<T>` and `k_local_type_id_v<T>` expose an explicitly
LOCAL binding. `TTypeId<T>` and `k_type_id_v<T>` select the registered category
and produce the category-bearing `type_id`; an unregistered type has no valid
selector.

All strong ID wrappers default to their canonical undefined value. Conversion
to the underlying integer representation is deliberately unavailable; ABI,
serialization, formatting, and dispatch code must request `raw_value()`
explicitly.

## Runtime Identity

Runtime identity uses one 64-bit alternating-bit layout with independent
fields for thread, mount-point, and module ordinals. A module ID combines a
mount-point field with a module field, and a system ID combines a module ID
with a thread field. The fields contain 16, 6, and 10 payload bits respectively.

Each field reserves zero as undefined and maps ordinal `n` to
`spread_to_even_bits(payload_mask - n)` at its field offset. Consequently every
ordinary ordinal from zero through `payload_mask - 1` has a distinct nonzero
encoding, and decoding every valid field reproduces its original ordinal.
Generated mount-point, module, and thread definition counts are checked against
those capacities at compile time.

Structural validity means that an ID is nonzero, contains no bits outside its
declared fields, and has every required constituent field. Registration is a
separate stronger property established by lookup in the installed system
registry.

## Local Definitions

Each binary owns an immutable local definition table. The current physical
layout keeps host definitions under `host/` and application definitions under
`modules/application/`; this does not anticipate the later source-layout
migration.

Component `.def` files enumerate ordinary zero-based ordinals. Repeated
inclusion generates IDs, C++ bindings, immutable registrations, and count
assertions. Names come from stringified identifier tokens and contain at most
15 bytes plus a terminator. Their complete 16-byte values are constructed at
compile time, terminated, and zero-filled.

The component installs and validates its complete table during explicit
bootstrap. Lookup revalidates category, decoded ordinal, range, exact entry
identity, termination, and zero fill. Missing, fabricated, corrupt, and
unavailable registrations resolve to failure without allocation or dynamic
registration.

The host TGA continuation states and application TGA continuation states are
stored only in their component-owned `CASyncStates`. They therefore belong to
their respective local tables rather than the system table. `TErasedPod` and
`CASyncState` carry `type_id`, retain their existing fixed layouts, and accept
either registered binding category.

Destination-aware admission belongs to the concrete erased-message and
erased-owner wrappers, not the generic queue and owning transport templates.
Each concrete wrapper stores its destination module. On `post()`, the source
is the ambient module: a registered SYSTEM identity may be admitted to any
valid destination, while a registered LOCAL identity requires source and
destination to be the same module. Undefined, malformed, unavailable, and
unregistered identities are rejected before copying, moving ownership, or
changing memory attribution. Retrieval does not revalidate identity; owning
retrieval only performs its required exit-side memory re-attribution.

Owning wrappers additionally require the declared destination to match the
module represented by the recipient context, or by the transport context when
there is no distinct recipient. `SErasedMsgHeader`, `CErasedPodMsg`,
`CErasedOwnerMsg`, `CErasedOwner`, and `CAssetRecord` expose category-bearing
identity without changing their established fixed layouts. `CErasedOwner`
supports explicitly eligible SYSTEM and LOCAL payloads. SYSTEM operations are
compiled into every receiving component; LOCAL operations exist only in the
defining component. Direct LOCAL-owner reattribution also requires source and
target contexts to belong to that component, independently of transport
admission.

Message dispatch compares the umbrella value locally. An unrecognised value
must be explicitly extracted as SYSTEM before it can enter the SYSTEM-only
`UnrecognisedMsg` response or existing debug argument transport. Unknown LOCAL
identity is neither echoed nor formatted, so the reserved local-type debug
codes remain unsupported.

## System Name Authority

Only the executable hosts system type, mount-point, module, thread, and
composed system names. Modules retain compiled numeric constants required by
the ABI but contain no independent system-name arrays.

The host installs an immutable view containing host-lifetime pointers and
fixed-width counts. Shared lookup code is compiled into each component and
operates over its component-local copy of that view. Before installation,
lookups safely return unresolved. Global names have no local short-name limit.
View validation checks table shape and encoded capacity before traversal, then
requires each entry to match its ordinal, encoded ID, relationship fields, and
name. The public installed-registry surface exposes the immutable view,
validated lookup and formatting, and one complete `validate_all()` diagnostic;
it does not duplicate raw table access or per-table validation APIs.

The debug argument codes reserved for local-type text and local-type failure
remain unsupported. Activating them and copying validated local names into
events belong to the later transport phase.
