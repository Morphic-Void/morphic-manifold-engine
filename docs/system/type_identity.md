Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

# System And Local Type Identity

## Categories

System type identities are the deliberate cross-component identity category.
Any C++ type used by a shared ABI, function query, erased payload, owner,
message, transport, or another component boundary requires a host-authored
system ID. `system/system_type_ids.def` is the sole canonical definition list.
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

`system_type_id` and `local_type_ids::id_type` are distinct strong C++
types. Their validators also reject raw encodings from the other category.

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

## System Name Authority

Only the executable hosts system type, mount-point, module, thread, and
composed system names. Modules retain compiled numeric constants required by
the ABI but contain no independent system-name arrays.

The host installs an immutable view containing host-lifetime pointers and
fixed-width counts. Shared lookup code is compiled into each component and
operates over its component-local copy of that view. Before installation,
lookups safely return unresolved. Global names have no local short-name limit.

The debug argument codes reserved for local-type text and local-type failure
remain unsupported. Activating them and copying validated local names into
events belong to the later transport phase.
