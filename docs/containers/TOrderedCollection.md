Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TOrderedCollection.md  
Author: Ritchie Brannan  
Date:   1 Apr 2026  

# TOrderedCollection<TKey, T>

## Overview

TOrderedCollection<TKey, T> is a move-only ordered collection wrapper
over TOrderedSlots and TStableStorage.

Slot metadata and ordering are managed by TOrderedSlots. Object storage
is provided by TStableStorage.

Public identity during the mutable phase is slot_index.

Constructed objects have stable addresses in TStableStorage.

sort_and_pack() remaps slot metadata, slot-side payload, and keys in
lock-step but does not relocate live T objects. Pointers and references
to constructed objects remain valid across sort_and_pack().

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.
- Indices, sizes, and capacities are expressed in elements.
- TKey must be trivially copyable.
- Live keys are unique.

Scope:

- Provides slot-based identity, key-based ordering, and stable object
  storage.
- Does not expose storage_index as public identity.
- Does not define ordering beyond the provided key comparator.

## State model

Each slot is in one of the following states:

Unmapped:

- slot has a valid storage_index binding
- backing storage has not been mapped

Mapped:

- backing storage is mapped
- no constructed object exists

Constructed:

- a live T object exists at the bound storage_index
- a live key is present and participates in ordered lookup

Only constructed slots expose live objects or keys.

Empty slots may retain valid internal storage_index bindings.

## Identity model

slot_index:

- public identity during mutation
- not stable across sort_and_pack()

storage_index:

- internal mapping to stable storage
- not exposed as public identity

## Storage and stability

Objects are placement-constructed in TStableStorage.

- addresses are stable once constructed
- slot and key remapping does not relocate objects
- storage_index determines object location

## Ordering model

Ordering is defined by TOrderedSlots.

- ordering applies to constructed slots with live keys
- ordered traversal follows comparator-defined order
- ordering is independent of storage layout

## Pack behaviour

sort_and_pack():

- reorders slot metadata, payload, and keys
- produces packed canonical slot layout
- does not move constructed objects
- preserves object addresses

## Lifetime model

Pointers and references returned by the collection:

- are non-owning
- refer to placement-constructed objects
- must not be destroyed with delete

Object lifetime must be managed through the collection API.

## Behavioural notes

- slot_index remapping does not imply object relocation
- internal storage_index bindings may exist for non-constructed slots
- key uniqueness is required for constructed slots
- collection is intended as a higher-level wrapper over ordering,
  slot, and storage primitives
