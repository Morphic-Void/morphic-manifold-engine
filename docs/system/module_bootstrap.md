Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

# Module Bootstrap ABI Version 3

The category-bearing type-identity transition removes component-confined
continuation types from the system identity table. Later system ordinals
therefore change and define a new DLL ABI. The exported entry point is
`morphic_module_bootstrap_v3`. Host and module exchange the same
`SAdvertisedIdentity` representation: claimed module ID, version, and inclusive
functional-major range. No version-1 compatibility structure or adapter is
retained because the DLL ABI is new and all components are rebuilt together.

`advertised_module_id` is deliberately named as such even within the advertised
identity structure: it is the component's claim and is distinct from the
ambient module identity assigned and installed later for execution.

The functional-major range describes the shared bootstrap/core protocol majors
with which each participant can interoperate. Binding requires the two ranges
to overlap and negotiates the highest common major. That selected major governs
the core function table and subsequent function queries. Host, module, and
functional majors are currently all 3.

The application validates and installs its immutable local-type table before
returning bootstrap functions. The host then exchanges numeric advertised
identities, populates the version-3 core surface, and installs services in this
order:

1. host-owned system-registry view;
2. ambient module identity;
3. module memory context;
4. debug service.

Only after all four services, the advertised peer identity, negotiated
functional major, and local table are installed is the module ready for
function queries or thread setup.
Thread identity, provisioning, and thread memory are installed later on each
created module thread.

Before registry installation, name lookup is unresolved and diagnostic paths
use numeric fallback. Any bootstrap failure leaves the module non-ready and
the host unloads it without starting module work.

Shutdown first stops and joins all module threads, then unloads the module.
The system registry view points to immutable host static storage and remains
valid for host lifetime, including while queued host-owned debug records drain.
No local identity is transported in this phase.
