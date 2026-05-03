Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   manifold_engine_interim_backlog.md  
Author: Ritchie Brannan  
Date:   3 May 26  

This represents the first run capture of the backlog after describing
my expected "to do" list to ChatGPT who I also asked to ask me clarifying
questions and perform some initial grouping.  

Some of the elements described here are already present, some of the questions
listed I already have answers for.  

This does represent what was in my head and in my notebooks regarding the design.  


# Manifold Engine Backlog - Interim Capture

**Status:** Interim backlog capture  
**Date:** 3 May 2026  
**Purpose:** Local archive, stepping-off point, and backup for known Manifold Engine backlog elements.

This document is an intentionally early backlog organisation pass. It captures known required systems,
desired components, infrastructure tasks, and higher-level future consumers. It does not attempt to fully
solve dependency ordering, implementation design, or task decomposition.

## Conventions captured

- **Cross-platform baseline** means:
  - Windows
  - Linux
  - macOS / OSX
  - Android

- **Host thread** means the default executable thread.
  - It creates and tears down the system.
  - During normal operation it acts as a service layer.
  - Main application logic runs on a host-created application/game thread.

- **Thread/module IDs** are static role/location identifiers.
  - They do not contain generation information.
  - They are not lifetime-validating handles.
  - They are static throughout the codebase.

- **All threads are host-created.**
  - Native creation is provided by the platform module.
  - Provisioning, identity, TLS, transports, and lifecycle ownership remain host responsibilities.

- **Host Registry** is the ultimate owner of shared services and backing resources.
  - It exists once.
  - It lives on the host thread.
  - It physically owns all standard SPSC transports.
  - It issues opaque 64-bit handles and permissions.

- **Debug reporting** does not use the standard SPSC transports.
  - It has its own dedicated communication path, likely MPSC and atomic.
  - It uses static thread/module IDs and TLS context.

- **JSON consumes text from the low-level text ingester.**
  - The JSON parser receives standards-compliant UTF-8.
  - Host / Host Registry remains the ultimate source of truth for actual file paths.

- **The 2-phase parking gate is only a parking mechanism.**
  - It reduces processor load and thread pressure.
  - It is not a correctness-bearing synchronisation primitive.

---

# 1. Foundation / bootstrap

## 1.1 Allocator bootstrap ordering

**Type:** Required bootstrap task.

**Purpose:** Ensure the system allocation path is installed before any system allocation, module load, registry creation, or thread creation.

### Scope

- Define exact allocator bootstrap order.
- Implement host-thread allocation object.
- Install host-thread allocation object before Host Registry creation.
- Ensure Host Registry depends on the installed allocator.
- Ensure modules are not loaded before allocator routing is installed.
- Ensure other threads are not created before allocator routing is installed.

### Notes

This must be completed before the temporary fallback allocator can be removed.

---

## 1.2 Temporary fallback allocator removal

**Type:** Required memory/bootstrap hardening task.

**Purpose:** Replace the temporary low-level fallback allocator with a hard invariant/fatal debug capture path once sufficient debug support exists.

### Current state

- Low-level memory management has a temporary fallback allocator.
- This exists only as a development/bootstrap convenience.

### Target behaviour

Once the host-thread allocation object is installed and the debug system can report violations:

- Reaching the fallback allocation path becomes a hard invariant violation.
- The debug system records the violation.
- The host initiates controlled shutdown.
- The fallback allocator is no longer a hidden recovery path.

### Tasks

- Preserve temporary fallback allocator only until bootstrap and debug support are ready.
- Add invariant check for unreachable fallback use.
- Route violation to debug fatal capture.
- Trigger controlled host teardown.

---

## 1.3 Baseline exception-free mutex

**Type:** Required pre-platform threading primitive.

**Purpose:** Provide a baseline exception-free mutex available before the platform module is loaded.

### Scope

- Lives in the pre-platform threading layer.
- Sits alongside abstracted threading primitives such as SPSC transports.
- May be used by the platform module as a fallback or interface-test implementation.
- Must expose a `noexcept` interface.

### Allocation policy

- Should generally not allocate during initialisation.
- Platform-native implementations may internally allocate if required.
- Such platform allocation must remain contained and must not use the main codebase allocator.

### Later clarification

- Exact operations:
  - initialise / destroy
  - lock / unlock
  - try-lock
  - timed lock, if any

---

## 1.4 Baseline 2-phase parking gate

**Type:** Required fallback parking primitive.

**Purpose:** Provide a minimal always-available thread parking mechanism to reduce processor load and scheduler pressure.

### Model

- Two mutexes.
- Simple `0/1` phase/index state.
- All participating threads observe the phase.
- Controller flips/toggles the phase to release or redirect waiting threads.

### Scope

- Pure parking mechanism only.
- Not a correctness-bearing synchronisation primitive.
- Not used for ownership transfer, work publication, queue correctness, or memory ordering correctness.
- Should wrap an already-correct polling/work loop.

### Platform integration

The platform module may provide better implementations using:

- wait-on-address primitives
- futex-style primitives
- semaphores
- other native park/wake mechanisms

The platform implementation may replace the mutex-based internals or replicate the baseline behaviour.

---

# 2. Threading / scheduling / transports

## 2.1 Static system IDs

**Type:** Required identity infrastructure.

**Purpose:** Centrally define static role/location IDs for modules and threads.

### Scope

- Define one shared `system_ids`-style header.
- Define static thread role IDs.
- Define static module IDs.
- IDs identify role/location only.
- No generation or lifetime validation is implied.

---

## 2.2 Host-created thread wrapper / core thread model

**Type:** Required core threading infrastructure.

**Purpose:** Define engine thread representation, TLS setup, identity, service access, and baseline transport provisioning.

### Thread role model

- Thread roles are fixed at compile time.
- Some conceptual or virtual roles may share one physical thread.
- Shared conceptual roles use the physical thread identity.
- The system may define many possible worker roles.
- Not all possible roles need to exist in every environment.

### Host-owned creation model

- All engine threads are host-created.
- The platform module creates native threads.
- The host provides all provisioning and host-system knowledge.
- The host manages all thread lifecycle.

### Mandatory early threads

The host always creates at least:

- application/game thread
- one or two background task threads for file loading and conditioning

Other systems may request additional threads, coordinated through the application thread, but the host remains the creator and owner.

---

## 2.3 TLS template and population

**Type:** Required thread wrapper subcomponent.

**Purpose:** Provide thread-local identity and per-thread service access for engine code.

### Scope

- TLS template belongs to the thread wrapper system.
- Host populates TLS during thread initialisation.
- TLS provides access to the current static thread ID and relevant module/thread context.
- Debug system uses enough thread topology information to provision support but does not own the TLS model.

### Consumers

- Generic engine code.
- Debug macros/reporting.
- Module interfaces.
- Thread service access.
- Transport access.

---

## 2.4 Standard SPSC transport provisioning

**Type:** Required communication infrastructure.

**Purpose:** Provide baseline host-to-thread and thread-to-host SPSC communication paths.

### Scope

- Host provisions all thread transports.
- Baseline transports are always provided.
- Additional transports are provided based on the thread creation request.
- Direct thread-to-thread transports are later optional topology support, not a first implementation task.

### Host ownership

- Host Registry physically owns all standard SPSC transports.
- Temporary oversight authority may be delegated.
- Ownership is not transferred away from the Host Registry.

---

## 2.5 Transport endpoint identity update

**Type:** Required update to existing SPSC transports.

**Purpose:** Add static identity context to each transport.

### Affected transports

Likely includes:

- `TRing`
- `TQueue`
- `TOwning`
- `TBulk`

### Required identity data

Each transport should record or expose:

- producer thread ID
- producer module ID
- consumer thread ID
- consumer module ID
- host/controller thread ID
- host/controller module ID, where relevant

### Notes

The host/controller thread is usually the main engine host thread, but that responsibility may sometimes be delegated.

---

# 3. Host ownership / registry / permissions

## 3.1 Host Registry

**Type:** Required core host infrastructure.

**Purpose:** Single authoritative host-side registry for handles, permissions, ownership, resource lifetime, and controlled cross-thread/module access.

### Core identity

- Exists exactly once.
- Lives only on the host thread.
- Distinct from other registry-like constructs.
- Does not contain policy decision logic.
- Records and enforces host-directed state.

### Responsibilities

- Issue opaque 64-bit handles.
- Issue and manage permissions.
- Manage access and lifetime.
- Revoke permissions.
- Disable existing interfaces to revoked resources.
- Place retiring handles/resources into a retirement-management backlog.
- Physically own all standard SPSC transports.
- Own long-term shared resources.
- Own backing resources that survive module teardown.
- Own temporary handle-addressed host work data.

---

## 3.2 Permissions and revocation

**Type:** Host Registry subcomponent.

**Purpose:** Control real runtime access and lifetime, not merely debug validation.

### Scope

- Permission records are managed by the Host Registry.
- Permissions may be revoked.
- Revocation may:
  - disable interfaces
  - move handles/resources to retirement management
  - prevent further access through controlled interfaces

---

## 3.3 Temporary handle-addressed host data

**Type:** Host service infrastructure.

**Purpose:** Support host-controlled multi-pass data handling using temporary handles.

### Example flow

- Request TGA image file.
- Host issues asynchronous file load.
- Loaded raw file returns to host.
- Host assigns a temporary handle.
- Handle is passed to data conditioning thread.
- Conditioning thread accesses raw data and decodes the TGA.
- Decoded result returns to host.
- Host may discard or archive the raw file buffer.
- If retained, raw data handle may be promoted into the Host Registry backing assets register.

### Scope

- Temporary data store.
- Handle assignment.
- Access permission.
- Promotion to backing resource/asset register.
- Retirement/discard handling.

---

## 3.4 Module teardown survival

**Type:** Primary host/registry responsibility.

**Purpose:** Ensure resources that must survive module teardown are retained by the host system.

### Scope

- Day-one requirement.
- Involves the Host Registry.
- Supports safe teardown of modules.
- Host makes policy decisions.
- Host Registry records and enforces ownership/lifetime state.

---

# 4. Diagnostics / debug

## 4.1 Debug system

**Type:** Required standalone core system.

**Purpose:** Centralised diagnostic, reporting, fatal capture, and controlled shutdown support.

### Ordering

- One of the first systems created.
- One of the last systems destroyed.
- Must exist before any worker/application thread creation.
- Depends on thread-system reporting/topology to provision worst-case thread support.

### Allocation policy

- May allocate during debug-system creation.
- After creation, performs no further allocation through the codebase allocator.
- Contained unavoidable OS/platform allocation may be tolerated where required.

### Communication model

- Does not use standard SPSC transports.
- Uses dedicated debug communication path.
- Likely MPSC and atomic.
- Threads report using static thread/module IDs and TLS context.

### Reporting scope

- Error reporting.
- Assertion/invariant failure reporting.
- Fatal diagnostic capture.
- Controlled shutdown trigger.
- Fallback allocator violation reporting.
- Thread-aware reports.
- Module-aware reports.

### Fatal handling

Fatal behaviour depends on severity.

If the reporting thread cannot safely continue:

- debug system may block that thread
- host is notified
- teardown proceeds without depending on the blocked thread

This should be rare; if reached, it indicates a significant invariant break requiring urgent correction.

### Output targets

Early safe output targets may include:

- debugger output
- console/log-style output where available
- file output if safe
- native popup for fatal paths, initially possibly Windows-only

Later output targets:

- in-engine overlay
- rendered UI reporting once game/editor rendering is available

---

# 5. Platform / OS integration

## 5.1 Minimal executable platform shim

**Type:** Required bootstrap platform support.

**Purpose:** Provide only the minimal platform support built into the executable.

### Scope

There is always a minimal built-in platform shim, but not all facilities are available.

Notably absent from the minimal shim:

- thread creation
- controller support
- keyboard support
- mouse support
- hardware-device-level input use

---

## 5.2 Platform system module

**Type:** Required cross-platform service module.

**Purpose:** Provide platform-specific services required by host, rendering, input, threading, and OS-facing UI behaviour.

### Baseline platforms

- Windows
- Linux
- macOS / OSX
- Android

### Core responsibilities

- Windows message pump and equivalent platform event loops.
- Native thread creation.
- Native park/wake/semaphore primitives.
- Native window creation for rendering.
- Popup/dialog handling.
- Concrete input device ingestion:
  - controller
  - keyboard
  - mouse where available/applicable

### Ownership boundaries

- Platform module creates native threads.
- Host provisions threads.
- Platform module does not own host topology knowledge.
- Concrete input ingestion belongs to the platform system.
- Host conditions input before provisioning it to other threads.

### Window/message pump

Currently treated as closely coupled, especially from the Windows perspective. Final separation may be platform-driven.

### Popup/dialog handling

Needed for:

- development
- fatal conditions

Once game/editor rendering is running, most popup-like reporting should move to the rendering system.

---

# 6. Text / encoding / source ingestion

## 6.1 Low-level text ingester

**Type:** Required low-level infrastructure component.

**Purpose:** Ingest raw text into confirmed-good UTF-8 while normalising line endings and reporting source characteristics.

### Scope

- Configurable strictness settings.
- Most use cases use maximum strictness.
- Always normalise line endings.
- Provide confirmed-good UTF-8 output.
- Produce an ingestion report.

### Ingestion report

The report should describe:

- what was encountered
- whether anything was corrected
- how corrections were applied
- line count
- maximum line length
- similar structural statistics
- non-standard/deviant UTF categories detected

### Non-standard UTF handling

Detect and categorise:

- Java-style nulls
- overlong encodings
- other decodable but non-standard forms

These categories remain associated with the ingested file for downstream policy decisions.

### Purpose of metadata

Allows later systems to decide whether the ingested data may enter a given path.

Examples:

- strict JSON parsing
- source ingestion
- localisation
- tool-only handling
- quarantine/rejection for strict consumers

---

## 6.2 Source code ingestion

**Type:** Text ingester consumer.

**Purpose:** Use the same low-level ingester for source-like text.

### Scope

- Source code ingestion.
- Shader source ingestion.
- Possible later connection back to JSON system.
- Ingested source may be represented in JSON metadata or conditioned assets.

---

# 7. Structured data / schema / generation

## 7.1 JSON system

**Type:** Large foundational structured-data system.

**Purpose:** Backbone for configuration, localisation, non-fast-path state capture/reconstruction, code generation, graphics metadata, and validation.

### Initial layer

- JSON internal dynamic representation.
- JSON parser.
- JSON writer.

### Parser input

- Consumes text from the low-level text ingester.
- Receives standards-compliant UTF-8.
- May allow only minor strictness relaxation around permissive normalisation.
- No broad JSON-extension model is implied at this stage.

### Diagnostics

JSON parse/schema diagnostics retain:

- line number
- column number
- JSON hierarchy context string

### Later/layered extensions

- Schema system.
- Structure definitions for code generation.
- Graphics reflection metadata.
- Validated graphics state descriptions.
- Configuration.
- Localisation data.
- Non-fast-path state capture/reconstruction.

---

## 7.2 File path handling in JSON

**Type:** JSON/host boundary rule.

**Purpose:** Clarify that JSON may contain path-like values without becoming the authoritative file path system.

### Scope

- JSON may include file paths.
- JSON may include shader source code as strings.
- JSON may include metadata that references source or asset files.
- JSON-local path interpretation may exist.

### Authority

The Host / Host Registry remains the ultimate source of truth for actual file paths, permissions, canonicalisation, and resolution.

---

## 7.3 Code generation / schema-backed structure definitions

**Type:** Later JSON layer.

**Purpose:** Use JSON schemas and structure definitions for generated code and reflection metadata.

### Scope

- Structure definitions.
- Code generation inputs.
- Graphics reflection support.
- Validated graphics-state support.
- Potential editor/tooling integration.

---

# 8. Maths / geometry

## 8.1 Maths system

**Type:** Large layered maths and geometry system.

**Purpose:** Provide foundational maths, geometry primitives, spatial queries, transforms, and joints.

### Low-level layer

- Vector maths library.
- Basic geometric structures:
  - rects
  - boxes
  - bounds
  - regions

### Higher-level geometry layer

- Circumspheres.
- Inspheres.
- Vector/ray collision tests against primitives:
  - boxes
  - spheres
  - planes
  - triangles
  - quads
  - similar simple primitives

### Intersection results

May include:

- `t` value
- point of intersection
- normal at intersection
- reflection at intersection

### Penetration tests

May include:

- penetration depth
- shortest resolution distance
- resolution direction

### Other tools

- Frustum tools.
- Standardised transforms.
- Standardised joints.

### Ownership boundary

Collision/intersection primitives are standalone maths/geometry libraries beneath physics.

Consumers include:

- physics
- rendering/GPU systems
- game logic
- editor/tools
- other spatial systems

---

# 9. Rendering / graphics pipeline

## 9.1 Graphics pipeline conditioning and validation

**Type:** Graphics/tooling module; separable from day one.

**Purpose:** Prepare and validate graphics pipeline data for runtime and offline use.

### Scope

- Shader reflection.
- PSO metadata creation.
- JSON-backed graphics metadata.
- Validated graphics state.
- API-specific pipeline state conditioning.
- API-agnostic pipeline conditioning.
- Runtime use.
- Offline asset pipeline use.

### Architectural requirement

The renderer should not care whether pipeline data came from:

- offline conditioning
- runtime/on-demand conditioning

---

## 9.2 Graphics asset conditioning

**Type:** Graphics/tooling module; separable from day one.

**Purpose:** Prepare graphics assets for runtime and offline use.

### Scope

- Image format conversion.
- Compression.
- Decompression.
- API-specific graphics asset conditioning.
- API-agnostic graphics asset conditioning.
- Runtime use.
- Offline asset pipeline use.

### Architectural requirement

This must remain separable from pipeline conditioning, even if bundled together in some builds or phases.

---

## 9.3 Rendering system

**Type:** Large hot-path runtime system; swappable module.

**Purpose:** Runtime rendering implementation.

### Scope

- Renderer layer.
- RHI layer.
- RHI thread request.
- Hot-path rendering.
- Vulkan baseline.
- DirectX baseline.
- Possible later APIs.
- Swappable rendering module.

### Thread ownership

- Rendering system may request an RHI thread.
- Host always owns, creates, provisions, and manages the thread.

---

# 10. Vector text / fonts / image annotation

## 10.1 Vector text and font system

**Type:** Standalone glyph-oriented component.

**Purpose:** Main user-facing text system for editor and game, also used by debug and prototyping.

### Consumers

- Game.
- Editor.
- Debug.
- Prototyping.
- Image capture annotation.
- Image sketch feedback.

### Scope

- Vector glyph model.
- Unicode-to-glyph mapping.
- User-facing text output.
- Shared glyph rendering path for multiple systems.

### Boundary

This is not part of the general text manipulation system. It is primarily glyph-based.

---

## 10.2 Image-buffer drawing/view layer

**Type:** Supporting image annotation component.

**Purpose:** Provide a simple view over existing rect byte buffers as images.

### Supported buffer formats

- 8-bit grayscale.
- 32-bit RGBA with 8 bits per channel.

### Drawing operations

- Lines.
- Boxes.
- Rects.
- Vector text output without anti-aliasing.

### Drawing modes

- Solid drawing.
- Transparent drawing.
- Small set of predefined blending operations.

---

# 11. Localisation / Katakana

## 11.1 Localisation system

**Type:** Significant engine system.

**Purpose:** Localised content handling, backed by JSON from the start.

### Scope

- JSON-backed localisation data.
- Offline asset pipeline support.
- External spreadsheet source may be conditioned into JSON.
- Smaller than rendering/graphics systems but still substantial.

---

## 11.2 Dedicated Katakana handling system

**Type:** Small specialised UTF-8 processing component.

**Purpose:** Provide narrow Katakana-specific string conditioning.

### Scope

- Bare-bones UTF-8 processor.
- String conditioning for meaningful user display.
- Agnostic to actual character rendering/display.
- Separate from localisation system.
- May provide localisation-adjacent support.

---

# 12. Tooling / editor-facing infrastructure

## 12.1 Offline asset pipeline

**Type:** Tooling infrastructure.

**Purpose:** Condition assets and metadata for runtime use.

### Scope

- Graphics pipeline conditioning.
- Graphics asset conditioning.
- Spreadsheet-to-JSON localisation conditioning.
- JSON-backed code generation.
- Source/shader ingestion.
- Image conditioning.
- Asset metadata generation.

### Requirement

Conditioned runtime data should match the shape of data produced by on-demand runtime conditioning so runtime consumers do not care where it originated.

---

## 12.2 Editor-facing utilities

**Type:** Tool/editor infrastructure placeholder.

**Purpose:** Support editor-facing rendering, text, image annotation, sketching, asset handling, and debug/prototyping workflows.

### Likely consumers

- Vector text/font system.
- Image-buffer drawing/view layer.
- JSON system.
- Maths/geometry.
- Rendering system.
- Graphics conditioning systems.
- Host Registry path/resource authority.

---

# 13. Higher-level consumer systems

These are intentionally left as placeholders in this interim document. They are distant implementation targets but should remain visible so infrastructure decisions are shaped by their eventual needs.

## 13.1 Game system

**Type:** High-level product/runtime system.

**Role:** The actual game/application layer consuming the engine infrastructure.

### Likely dependencies

- Platform system.
- Rendering system.
- Maths/geometry.
- Vector text/font system.
- Input/human interface.
- Threading/services.
- JSON/config/state systems.
- Physics system.
- Localisation/Katakana support where needed.

---

## 13.2 Editor system

**Type:** High-level tooling/application system.

**Role:** Main authoring environment for game/world/content/tool workflows.

### Likely dependencies

- Platform/windowing/input.
- Rendering system.
- Vector text/font system.
- Image annotation/sketch layer.
- JSON system.
- Graphics conditioning systems.
- Asset conditioning systems.
- Maths/geometry.
- Host Registry/resource ownership.
- File/path authority.

---

## 13.3 Physics system

**Type:** High-level runtime/simulation system.

**Role:** Simulation and spatial interaction system above maths/geometry primitives.

### Likely dependencies

- Vector maths.
- Transforms.
- Joints.
- Primitive intersection tests.
- Penetration tests.
- Threading/job services.
- JSON/config/state capture.
- Game/editor integration.

### Boundary

Maths collision/intersection primitives are not owned by physics. Physics consumes them and adds simulation policy, state, constraints, broad phase, narrow phase, and gameplay/editor-facing behaviour.

---

# 14. Current broad backlog map

```text
01 Foundation / bootstrap
02 Threading / scheduling / transports
03 Host ownership / registry / permissions
04 Diagnostics / debug
05 Platform / OS integration
06 Text / encoding / source ingestion
07 Structured data / schema / generation
08 Maths / geometry
09 Rendering / graphics pipeline
10 Vector text / fonts / image annotation
11 Localisation / Katakana
12 Tooling / editor-facing infrastructure
13 Higher-level consumer systems
```

---

# 15. Known unresolved areas for later passes

The following are intentionally not resolved in this interim capture.

## Dependency ordering

- Full bootstrap dependency chain.
- Thread/debug/registry creation order.
- Platform module load sequence.
- Rendering/platform/window creation sequence.
- Offline asset pipeline dependency graph.

## Task decomposition

Large systems still need decomposition into buildable milestones:

- JSON system.
- Maths system.
- Host Registry.
- Debug system.
- Platform system.
- Rendering system.
- Graphics conditioning systems.
- Vector text/font system.
- Physics system.
- Editor system.
- Game system.

## Interfaces and data models

Still to define:

- Host Registry handle layout and permission record format.
- TLS structure.
- Debug report payload format.
- SPSC transport endpoint descriptors.
- Thread creation request structure.
- JSON dynamic representation ownership model.
- Text ingestion report format.
- Graphics metadata schema.
- Vector glyph representation.
- Image-buffer drawing API.
- Localisation JSON schema.

## Build / module packaging

Still to decide:

- Which systems are static executable facilities.
- Which systems are swappable modules.
- Which systems are shared by runtime and offline tools.
- Which modules can be bundled while remaining architecturally separable.

---

# 16. Suggested next backlog passes

## Pass A: Bootstrap dependency chain

Goal: establish first safe creation order from process start to application thread creation.

Likely scope:

- executable platform shim
- allocator installation
- debug system creation
- Host Registry creation
- platform module load
- thread wrapper setup
- application thread creation

## Pass B: Threading and registry data model

Goal: define the identity, handle, transport, and lifecycle records required before implementation.

Likely scope:

- static system IDs
- TLS template
- thread role table
- transport descriptors
- opaque 64-bit handles
- permissions
- retirement backlog
- temporary host work data

## Pass C: Text / JSON first implementation slice

Goal: define a concrete first implementation path for source/config/localisation ingestion.

Likely scope:

- text ingester
- ingestion report
- JSON dynamic representation
- parser/writer
- parse diagnostics
- initial localisation JSON path

## Pass D: Graphics conditioning boundary

Goal: define the module boundaries between pipeline conditioning, asset conditioning, renderer, and offline tools.

Likely scope:

- API-agnostic pipeline conditioning
- API-specific pipeline conditioning
- API-agnostic asset conditioning
- API-specific asset conditioning
- JSON metadata schema
- renderer-facing conditioned data contract
