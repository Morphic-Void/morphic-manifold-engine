Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   TUnorderedCollection.md  
Author: Ritchie Brannan  
Date:   1 Apr 2026  

# TUnorderedCollection<T>

## Overview

TUnorderedCollection<T> is a move-only unordered collection wrapper
over TUnorderedSlots and TStableStorage.

Slot metadata is managed by TUnorderedSlots. Object storage is provided
by TStableStorage.

Public identity during the mutable phase is slot_index.

Constructed objects have stable addresses in TStableStorage.

pack() remaps slot metadata and slot-side payload but does not relocate
live T objects. Pointers and references to constructed objects remain
valid across pack().

## Requirements and scope

- Requires C++17 or later.
- No exceptions are used.
- Indices, sizes, and capacities are expressed in elements.

Scope:

- Provides slot-based identity and stable object storage.
- Does not expose storage_index as public identity.
- Does not define ordering or rank semantics.

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

Only constructed slots expose live objects.

Empty slots may retain valid internal storage_index bindings.

## Identity model

slot_index:

- public identity during mutation
- not stable across pack()

storage_index:

- internal mapping to stable storage
- not exposed as public identity

## Storage and stability

Objects are placement-constructed in TStableStorage.

- addresses are stable once constructed
- slot remapping does not relocate objects
- storage_index determines object location

## Traversal model

Traversal follows TUnorderedSlots behaviour.

- traversal order is list order
- traversal order does not imply rank
- traversal order does not imply insertion order

## Pack behaviour

pack():

- compacts slot metadata
- remaps slot_index values
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
- collection is intended as a higher-level wrapper over slot and
  storage primitives
