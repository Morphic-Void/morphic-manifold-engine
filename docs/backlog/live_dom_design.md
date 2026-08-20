Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

File:   live_dom_design.md
Author: Ritchie Brannan
Date:   20 Aug 2026

# Live DOM design

## Status and purpose

This document records the intended design direction for TheMorphicEngine's
document substrate.  It is an implementation reference and a statement of
scope, not an implementation specification frozen against future evidence.

The immediate concern is a mutable, JSON-shaped live document object model.
Its principal workload is programmatic construction and editing: mutation,
addition, replacement, detachment, and pruning.  JSON parsing is a secondary
importer of that model, not the mechanism that defines it.

The document substrate will later support schema interpretation, shader and
pipeline configuration, consolidation, and construction of real-time data.
Those systems are consumers of this work.  They are not defined by this
document or implemented as part of the initial live-DOM work.

## Threading and quiescence

The live document has no internal synchronization.  It is modified by one
thread at a time.  Read access is permitted only while mutation is prohibited
and the document is quiescent; lifetime authority and any required external
coordination remain with the caller.

Read operations must not lazily create caches or otherwise mutate the
document.  Any such cache is mutation and is permitted only under the same
single-threaded mutation contract, or must be built before the document is
published for quiescent access.  The baked representation is immutable after
construction and likewise has no need for internal write synchronization.

## Representation lifecycle

The same durable document meaning has three deliberately different forms:

```text
JSON text
  durable human-readable audit, debugging, and archival representation
        |
        | parse or programmatic construction
        v
Live DOM
  mutable editor/build representation
        |
        | bake
        v
Baked DOM
  immutable binary distribution and construction representation
        |
        | instantiate
        v
Real-time data
  domain-specific execution data consumed by the distributed product
```

JSON is the long-term durable form.  The live form owns mutable construction
and relationship editing.  The baked form is an immutable, compact,
relatively fast structural form used mainly to construct real-time data; it
is not itself the final real-time representation.  A real-time domain may
retain useful baked indices as IDs, but that is a deliberate domain-local
choice rather than a general identity guarantee.

The live and baked forms are stateless across their lifecycles.  Neither live
keys nor baked indices are serialized as document identity.  Durable identity
and references must be recoverable from document content.

## Live DOM scope

The live DOM models ordinary JSON kinds:

- null
- boolean
- integer
- floating-point number
- string
- array
- object

All node kinds use one common tagged, trivially-copyable slot payload.  The
tag determines the interpretation of a small common payload field set.  Scalar
values are embedded in the slot.  Array and object nodes embed their child-list
header in the same payload footprint.  This avoids per-kind heap object
models, vtables, allocation chasing, and separate structural sidecars.

An illustrative shape is:

```cpp
struct CJsonSlot {
    NodeKey parent;
    NodeKey previous_sibling;
    NodeKey next_sibling;
    PropertyNameId name_in_parent;
    uint32_t type_and_flags;
    CJsonPayload payload;
};
```

The final layout must be selected from actual access and packing requirements.
It should use natural alignment, fixed-width fields, and a power-of-two record
size.  A 64-byte live-slot target is appropriate unless actual payload needs
show otherwise.  The power-of-two target is a low-cost consistency rule, not a
claim of a material automatic performance gain.

`CJsonPayload` is a POD union.  Scalars use an embedded value or stable string
ID.  Arrays and objects use an embedded child-list header:

```cpp
struct CChildList {
    NodeKey first;
    NodeKey last;
    uint32_t count;
    uint32_t revision;
};
```

Object and array children are a linear, key-linked list.  A child holds its
parent, previous sibling, and next sibling; an object child also stores its
`name_in_parent`.  Array children and the root use an invalid name sentinel.
Object and array order are semantic and must be preserved.

Lists are never circular.  `first.previous_sibling` and `last.next_sibling`
are invalid.  This makes termination and integrity validation explicit.  A
caller that needs wraparound can reach the parent and then its first or last
child without introducing circular sibling links.

The live representation favors inexpensive mutation over random positional
access.  Append, insertion relative to a known sibling, detach, and sibling
traversal are direct key-link operations.  Positional array lookup is O(N).
Callers performing sequential array access may retain a revision-bound cursor
containing the parent, current child, and current index; the next element is
then reached directly through `next_sibling`.  A parent child-list revision
invalidates such cursors after structural mutation.

## Live keys and slots

The live node registry is expected to use `TPodOrderedSlots` with an
externally generated monotonically increasing 64-bit key.  The key identifies
a node only during one live-document lifecycle.  Its purpose is to make
physical slot reuse safe:

```text
old node: slot 17, key 1001
erase old node
new node: slot 17, key 1002
lookup 1001: not found
lookup 1002: found
```

The ordered-key lookup provides this stale-key rejection without requiring a
generation field in every slot.  Slot position remains an implementation
detail.  `sort_and_pack()` may rearrange physical storage without changing the
meaning of live relationships held as keys.

`TPodUnorderedSlots` with a `{ slot_index, generation }` handle is a viable
alternative for direct mutable access, but it requires explicit generation
maintenance and remapping of all handles if a live store is packed.  The
ordered-key arrangement is the current preferred live model.

Keys are never written to JSON or baked binary output, never used as durable
references, and never preserved by bake, load, or promotion.  A promoted
document creates a fresh live-key universe.

## Names, strings, and efficient lookup

`CStableStrings` is suitable backing storage for document strings, but string
roles must remain separate:

```text
PropertyNames   object member names and canonical path name segments
StringValues    ordinary JSON string scalar content
CanonicalPaths  canonical reference path records and, where needed, locators
```

Property-name interning turns repeated structural comparisons into integer-ID
comparisons.  An arbitrary author string is looked up lexically once at the
text boundary; subsequently, object member lookup operates on a
`PropertyNameId`.  The initial live implementation may scan an object's
linked children, comparing integer IDs.  A revision-bound lookup cache is a
future optimization only if workload evidence justifies it.

Each live string-related table has a companion use-count table.  Counts are
updated atomically with document mutation and identify semantically live
entries for baking.  A zero-use value is not removed from the live
`CStableStrings` table: live IDs remain stable for the document lifecycle.
During baking, only live entries are copied into compact baked tables and
rewritten through live-to-baked string/name maps.  A structural audit verifies
that recorded use counts match actual graph use.

## Navigation and references

Navigation has two forms.

Traversal paths are operational and transient.  They may use object names and
array indices, for editor selection, diagnostics, and mutation operations.

Durable reference paths may ascend through structural parents and descend only
through object member names.  They must not descend through arrays using an
array index.  Array insertion or deletion changes positional indices and must
not silently retarget a durable reference.

```text
transient:  #/materials/3/states/0
durable:    ../sharedMaterials/default
```

Canonical durable paths are represented as interned property-name ID steps and
parent steps, optionally prefixed with a canonical asset/document locator.
They are reconstructable as a long textual form.  Resolution to a live key or
baked index is only a lifecycle-local cache.

If an array element needs durable direct identity, it must be represented by a
named object member or a future schema-defined semantic-ID lookup.  It must
not be identified by array ordinal position.

## Object-name uniqueness

Within an individual object, immediate member names are unique by exact byte
sequence.  The same name may occur freely in different objects at different
levels of the document.

```json
{
  "render": { "settings": {} },
  "audio":  { "settings": {} }
}
```

Duplicate `settings` members in one object are invalid.  Normal mutation
operations reject duplicate creation or a colliding rename atomically; they do
not silently overwrite or transform data.  Replacement is a distinct
operation and requires an existing member.

## Diagnostics and malformed JSON recovery

Document diagnostics are metadata held by the document instance, not semantic
JSON nodes.  They record status, failure code, severity, relevant property
name, and source locations where available.

Normal JSON import rejects duplicate object members and reports both source
locations.  An explicit diagnostic recovery importer may instead construct a
distinct `RecoveredDuplicateArray` node.  It has ordinary array traversal
mechanics but is a separate node variant that records collision metadata.  It
is created only by recovery import and cannot be created by ordinary mutation.

Recovered documents are non-standard.  They exist solely to inspect and
repair malformed input.  A diagnostic JSON writer may serialize them with an
explicit named recovery object, for example under `$morphic.recovery`; this is
not a valid engine document specification and normal parsing does not assign
it special meaning.

## Purity and baking

Only a pure live document may bake.  A pure document contains only standard
node kinds and has no blocking diagnostics or recovery state.

```text
pure live document                 -> bake permitted
recovered/non-standard document    -> bake rejected
fatal/invalid document             -> bake rejected
```

The baked representation contains no live keys, slots, free-list state,
recovery nodes, or recovery metadata.  It assigns dense baked indices and
rewrites linked live children into dense contiguous array/member ranges for
O(1) indexed access.  It strips unused strings and names.  Its node records
should also use natural alignment, fixed-width fields, and a deliberate
power-of-two size, normally 32 or 64 bytes as the actual binary payload and
query profile require.  Loading binary creates a baked document; editing
requires explicit promotion into a new live document.

## Interface boundaries

Live and baked documents do not inherit from a common document class and do
not use virtual functions for DOM access.  They expose compatible read method
signatures so common traversal and validation logic can use static dispatch or
parallel implementations without confusion.

The JSON parser targets the live document only.  The JSON writer may write
from either representation through a small virtual read-only write-source
adapter.  This virtual adapter is confined to boundary IO and does not shape
either DOM's native access model.

## Deferred layers

Schema and high-level domain systems sit above the live DOM.  Schema may be
JSON-described and may interpret a JSON string as a domain value, such as an
exact float bit representation, vector, matrix, or compact array.  Such
interpretation is schema-directed; the base DOM does not infer it globally or
gain special node kinds for it.

The future real-time data stage is outside this project's scope.  This layer
must provide it a reliable structural surface, not decide its final resource,
renderer, or execution-object design.

## Development roadmap

1. Define the common live slot record, live key wrapper, and ordered slot
   registry contract.
2. Implement embedded linear child lists, structural read traversal, and
   revision-bound sequential access cursors.
3. Implement creation, attachment, replacement, detachment, and subtree
   pruning.
4. Define structural ownership: object/array edges own children; canonical
   references are non-owning and may become unresolved after pruning.
5. Implement separate stable string pools and transactional use-count updates.
6. Implement integrity audits and tests for slot reuse, stale-key rejection,
   ordering, pruning, collision rejection, and count consistency.
7. Add JSON parse/write adapters, including explicit diagnostic recovery mode.
8. Design baking from the proven live-DOM behaviour, then add pure-document
   validation, dense index construction, compaction, binary streaming, and
   promotion.

The first implementation milestone is not JSON parsing.  It is a robust live
document that can be constructed, mutated, navigated, pruned, and audited
without violating its ownership, identity, string-use, or object-name
invariants.
