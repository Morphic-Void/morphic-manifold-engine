Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   manifold_engine_interim_backlog.md  
Author: Ritchie Brannan  
Date:   Updated 12 May 26  

This represents the second run capture of the backlog.

The capture was originally created by ChatGPT after describing
my expected "to do" list to it and after having it ask me clarifying
questions and perform some initial grouping.  

Updates and maintainence of this backlog also heavily use ChatGPT.

Some of the elements described here are already present, some of the questions
listed I already have answers for.  

This does represent what was in my head and in my notebooks regarding the design.  


# Manifold Engine Backlog - Interim Capture v2

**Status:** Updated interim backlog  
**Purpose:** Local archive, stepping-off point, and backup for known Manifold Engine backlog elements.  
**Update intent:** Incorporates changes since the first interim capture, removes completed items from the active backlog, and reflects the current short-term pivot toward text ingestion and JSON infrastructure.

This document remains an interim backlog. It is not a full dependency graph, schedule, or implementation design. It is a scope and work-organisation aid.

---

# 0. Current direction

## 0.1 Short-term focus

The immediate focus is now platform-agnostic lower-layer buildout:

1. Low-level text ingester.
2. JSON infrastructure.
3. Debug system buildout in smaller interleaved slices.

The JSON work extends upward architecturally, but it is a single contained pillar and is therefore suitable as the main near-term focus.

## 0.2 Deferred but not abandoned

Remaining threading work is deferred, not abandoned.

The completed primitive layer is sufficient for now. Remaining thread provisioning, TLS, and higher thread lifecycle work naturally lead upward into larger integration, so they will resume when there is more useful functionality to connect to.

## 0.3 Platform module reframing

Threading support no longer lives in the platform module.

The contained cross-platform threading wrappers are sufficient as the native/threading primitive layer. The platform module is now primarily for:

- system/message pumps
- HID / input device handling
- window creation
- popup/dialog creation

Some popup creation may enter the main executable directly as part of debug infrastructure, especially for early or fatal reporting.

---

# 1. Completed since first interim capture

These items are complete and are removed from the active backlog.

## 1.1 FIFO discard-oldest / try_add work

**Status:** Complete  
**Scale:** Small/Medium  
**Domain:** Containers

Completed scope:

- Added optional full-buffer insertion policy.
- Added discard-oldest behaviour.
- Added `try_add()` function group matching `add()`.
- `try_add()` never overwrites.
- Added current policy query.
- HID-specific recovery behaviour kept out of FIFO implementation.

## 1.2 Cross-platform threading primitive / thread-start tranche

**Status:** Implementation complete  
**Domain:** Threading primitives

Completed implementation items:

1. Defined primitive wrapper boundary.
2. Implemented hardware thread count query.
3. Implemented mutex wrapper.
4. Implemented wait/wake wrapper or fallback-compatible wait primitive.
5. Decided semaphore was needed for this layer.
6. Implemented semaphore wrapper.
7. Implemented 2-phase parking gate.
8. Defined native thread entry contract.
9. Implemented native thread creation wrapper.
10. Implemented minimal thread start trampoline.

Validation status:

- Windows path has passed basic ad-hoc functional testing.
- Formal smoke/stress testing remains open and deferred.

## 1.3 Cross-platform high-performance counter

**Status:** Complete  
**Scale:** Small  
**Domain:** Timing / profiling foundation

Completed scope:

- Cross-platform high-frequency counter.
- Query current count.
- Query counter frequency.
- Suitable as low-level timing source for:
  - debug
  - profiling
  - frame timing
  - frame-rate display
  - throttling layers

---

# 2. Active short-term backlog

## 2.1 Low-level text ingester

**Status:** Active / primary near-term focus  
**Scale:** Medium  
**Domain:** Text / encoding / source ingestion

### Purpose

Ingest raw text into confirmed-good UTF-8 while normalising line endings and reporting source characteristics.

### Scope

- Build on SuiteUTF.
- Configurable strictness settings.
- Most use cases use maximum strictness.
- Always normalise line endings.
- Produce confirmed-good UTF-8 output.
- Produce an ingestion report.

### Ingestion report should include

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
- other decodable but non-standard UTF forms

These categories remain associated with the ingested file for downstream policy decisions.

### Consumers

- JSON parser
- source code ingestion
- shader source ingestion
- localisation data path
- code generation inputs
- tooling

---

## 2.2 JSON infrastructure - first pillar

**Status:** Active / primary near-term focus after or alongside text ingester  
**Scale:** Medium to Large  
**Domain:** Structured data / schema / generation

### Purpose

Provide the structured-data backbone for configuration, localisation, non-fast-path state capture/reconstruction, code generation, graphics metadata, and validation.

### Initial scope

- JSON internal dynamic representation.
- JSON parser.
- JSON writer.
- Parse diagnostics.

### Parser input

- Consumes text from the low-level text ingester.
- Receives standards-compliant UTF-8.
- May allow only minor strictness relaxation around permissive normalisation.
- No broad JSON-extension model is currently implied.

### Diagnostics

JSON parse/schema diagnostics should retain:

- line number
- column number
- JSON hierarchy context string

### Later JSON layers

- Schema system.
- Structure definitions for code generation.
- Graphics reflection metadata.
- Validated graphics state descriptions.
- Configuration.
- Localisation data.
- Non-fast-path state capture/reconstruction.

---

## 2.3 Debug system buildout

**Status:** Active but secondary / interleaved  
**Scale:** Medium to Large  
**Domain:** Diagnostics / debug

### Purpose

Centralised diagnostic, reporting, fatal capture, and controlled shutdown support.

### Current role in schedule

Debug system work will occur in smaller interleaved slices, both to provide variety from text/JSON work and to support diagnostics needed by text ingestion and JSON parsing.

### Ordering / lifecycle

- One of the first systems created.
- One of the last systems destroyed.
- Must exist before any worker/application thread creation once full threading resumes.
- Depends on thread-system reporting/topology to provision worst-case thread support.

### Allocation policy

- May allocate during debug-system creation.
- After creation, performs no further allocation through the codebase allocator.
- Contained unavoidable OS/platform allocation may be tolerated where required.

### Communication model

- Does not use standard SPSC transports.
- Uses a dedicated debug communication path.
- Likely MPSC and atomic.
- Threads report using static thread/module IDs and TLS context once that layer exists.

### Initial buildout areas

- Debug report payloads.
- Severity handling.
- Allocation-at-create storage.
- Initial fatal/invariant reporting path.
- Initial safe output targets.
- Support for text/JSON diagnostics where useful.

### Later output targets

- In-engine overlay.
- Rendered UI reporting once game/editor rendering is available.

---

## 2.4 TGA manual testing / lightweight validation

**Status:** Open  
**Scale:** Small/Medium  
**Domain:** Image codec validation

### Current leaning

Perform manual testing rather than building formal tests immediately.

### Scope

- Exercise representative TGA decode paths.
- Exercise representative TGA encode paths.
- Check format assumptions.
- Check orientation handling.
- Check grayscale, 24-bit, 32-bit, and RLE paths where practical.
- Defer formal automated test suite unless manual testing exposes issues or later tooling makes it cheap.

---

# 3. Deferred validation backlog

## 3.1 Threading primitive smoke/stress tests

**Status:** Deferred  
**Scale:** Small/Medium  
**Domain:** Threading validation

### Current status

- Windows path has passed basic ad-hoc functional testing.
- Formal smoke/stress coverage is still required.
- Testing is deferred until the codebase can exercise at least Windows and Linux paths.

### Coverage should include

- hardware thread count query
- mutex lock/unlock
- wait/wake single
- wait/wake all
- semaphore acquire/release
- 2-phase parking gate behaviour
- native thread creation/start/exit
- repeated initialise/destroy
- basic failure-state handling where testable
- cross-platform behaviour comparison between Windows and Linux

---

# 4. Threading / identity / provisioning backlog

The primitive/native wrapper layer is complete. The remaining threading work is deferred until lower-layer text/JSON/debug prerequisites are more useful.

## 4.1 Refactor module/thread IDs to separate role and sub-role/sub-identity

**Status:** Open  
**Scale:** Small/Medium  
**Domain:** Static identity model

### Purpose

Refine existing static module/thread IDs to separate primary role from sub-role/sub-identity.

### Scope

- Preserve static role/location semantics.
- No generation/lifetime identity implied.
- Support more precise naming, reporting, and provisioning.
- Feed later debug and thread provisioning structures.

---

## 4.2 Module, thread, and system names / ID registry

**Status:** Open  
**Scale:** Small/Medium  
**Domain:** Diagnostics / identity metadata

### Purpose

Provide human-readable names for static IDs.

### Scope

- Module names.
- Thread names.
- System names.
- ID registry for lookup and diagnostics.

### Likely consumers

- debug reports
- logging
- assertions
- thread naming
- profiling
- Host Registry diagnostics

---

## 4.3 Thread provisioning and data access structures

**Status:** Deferred  
**Scale:** Medium  
**Domain:** Threading tranche 2

### Purpose

Define the host-side data structures used to provision threads and expose per-thread data/services.

### Scope

- Thread provisioning records.
- Access structures for per-thread data/services.
- Host-created thread data made available to runtime code.
- Shape the data model before TLS is finalised.

---

## 4.4 TLS definition

**Status:** Deferred  
**Scale:** Medium  
**Domain:** Threading tranche 2

### Dependency

Depends on thread provisioning and data access structure decisions.

### Scope

- TLS template.
- Thread-local identity access.
- Access to provisioned per-thread data.
- Support for debug routing and generic code access.

---

# 5. Foundation / bootstrap backlog

## 5.1 Allocator bootstrap ordering

**Status:** Open  
**Scale:** Small  
**Domain:** Foundation / memory bootstrap

### Purpose

Ensure system allocation routing is installed before any system allocation, module load, registry creation, or thread creation.

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

## 5.2 Temporary fallback allocator removal

**Status:** Open  
**Scale:** Small/Medium  
**Domain:** Memory/bootstrap hardening

### Purpose

Replace the temporary fallback allocator with a hard invariant/fatal debug capture path once sufficient debug support exists.

### Target behaviour

Once the host-thread allocation object is installed and the debug system can report violations:

- Reaching the fallback allocation path becomes a hard invariant violation.
- The debug system records the violation.
- The host initiates controlled shutdown.
- The fallback allocator is no longer a hidden recovery path.

---

# 6. Host ownership / registry / permissions backlog

## 6.1 Host Registry

**Status:** Later / required  
**Scale:** Large  
**Domain:** Host ownership / registry / permissions

### Purpose

Single authoritative host-side registry for handles, permissions, ownership, resource lifetime, and controlled cross-thread/module access.

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

## 6.2 Temporary handle-addressed host data

**Status:** Later / required  
**Scale:** Medium  
**Domain:** Host services

### Purpose

Support host-controlled multi-pass data handling using temporary handles.

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

---

# 7. Platform / OS integration backlog

Threading support is no longer part of the platform module scope.

## 7.1 Windows platform module

**Status:** Later  
**Scale:** Medium/Large  
**Domain:** Platform / OS integration

### Scope

- Windows message/system pump.
- HID ingestion.
- Window creation.
- Popup/dialog support where appropriate.
- Rendering surface support where needed.

### Out of scope

- Native threading primitives.
- Thread creation.
- Mutex/wait/wake/semaphore wrappers.
- Thread provisioning.
- TLS setup.

---

## 7.2 Linux platform module

**Status:** Later  
**Scale:** Medium/Large  
**Domain:** Platform / OS integration

### Scope

- Linux event/system pump equivalent.
- HID/input ingestion.
- Window creation path.
- Popup/dialog strategy where appropriate.
- Rendering surface support where needed.

### Out of scope

- Native threading primitives.
- Thread creation.
- Mutex/wait/wake/semaphore wrappers.
- Thread provisioning.
- TLS setup.

---

## 7.3 Main executable popup/debug path

**Status:** Later / possible  
**Scale:** Small/Medium  
**Domain:** Debug / platform-adjacent

### Purpose

Allow some popup creation directly from the main executable as part of debug infrastructure, especially for fatal paths where the platform module or rendering system may not be available.

---

# 8. Build and platform validation backlog

## 8.1 Set up project to build on Linux

**Status:** Open  
**Scale:** Medium  
**Domain:** Build/platform enablement

### Purpose

Stand up Linux build support.

### Scope

- Verify existing theoretically-Linux paths.
- Enable basic compilation and linking.
- Begin making Linux path testable.
- Unlock formal Windows/Linux comparison for threading primitive tests.
- Prepare ground for Linux platform module later.

---

# 9. Maths / geometry backlog

## 9.1 Maths system

**Status:** Later / required  
**Scale:** Large  
**Domain:** Maths / geometry

### Purpose

Provide foundational maths, geometry primitives, spatial queries, transforms, and joints.

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
- Vector/ray collision tests against:
  - boxes
  - spheres
  - planes
  - triangles
  - quads
  - similar simple primitives

### Intersection results may include

- `t` value
- point of intersection
- normal at intersection
- reflection at intersection

### Penetration tests may include

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

# 10. Rendering / graphics pipeline backlog

## 10.1 Graphics pipeline conditioning and validation

**Status:** Later / required  
**Scale:** Large  
**Domain:** Graphics/tooling

### Purpose

Prepare and validate graphics pipeline data for runtime and offline use.

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

## 10.2 Graphics asset conditioning

**Status:** Later / required  
**Scale:** Medium/Large  
**Domain:** Graphics/tooling

### Purpose

Prepare graphics assets for runtime and offline use.

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

## 10.3 Rendering system

**Status:** Later / required  
**Scale:** Large  
**Domain:** Runtime rendering

### Purpose

Runtime rendering implementation.

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

# 11. Vector text / fonts / image annotation backlog

## 11.1 Vector text and font system

**Status:** Later / required  
**Scale:** Large  
**Domain:** Text rendering / glyphs

### Purpose

Main user-facing text system for editor and game, also used by debug and prototyping.

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

## 11.2 Image-buffer drawing/view layer

**Status:** Later / required  
**Scale:** Medium  
**Domain:** Image annotation

### Purpose

Provide a simple view over existing rect byte buffers as images.

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

# 12. Localisation / Katakana backlog

## 12.1 Localisation system

**Status:** Later / required  
**Scale:** Medium/Large  
**Domain:** Localisation

### Purpose

Localised content handling, backed by JSON from the start.

### Scope

- JSON-backed localisation data.
- Offline asset pipeline support.
- External spreadsheet source may be conditioned into JSON.
- Smaller than rendering/graphics systems but still substantial.

---

## 12.2 Dedicated Katakana handling system

**Status:** Later / desired/specialised  
**Scale:** Small  
**Domain:** UTF-8 text conditioning

### Purpose

Provide narrow Katakana-specific string conditioning.

### Scope

- Bare-bones UTF-8 processor.
- String conditioning for meaningful user display.
- Agnostic to actual character rendering/display.
- Separate from localisation system.
- May provide localisation-adjacent support.

---

# 13. Tooling / codebase hygiene backlog

## 13.1 Code sanitisation / codebase rule checker

**Status:** Later  
**Scale:** Medium initially, possibly larger over time  
**Domain:** Tooling / static-analysis-style support

### Purpose

Build a small code parser/scanner to detect unwanted language and architectural usage creeping into the codebase.

### Initial checks

- detect use of `new`
- detect use of `delete`, if not already implied
- detect exception usage
- detect `throw`
- detect `try`
- detect `catch`
- detect exception-bearing patterns where practical

### Supported invariants

- no exceptions
- no general `new` / `delete`
- allocation through the controlled memory layer
- module/host boundary discipline

### Likely later expansion

- undesirable code crossing module boundaries
- undesirable data crossing module boundaries
- misuse of host services
- incorrect ownership transfer patterns
- direct access where controlled interfaces are required
- forbidden includes in specific layers
- dependency direction violations
- accidental use of disallowed standard library facilities

### Timing

This should remain later backlog work because its useful shape depends on more progress in:

- module interfaces
- host service interfaces
- module boundary rules
- allocation rules in final form
- exception quarantine rules, if any external SDKs require them
- directory/layer structure

---

# 14. Higher-level consumer systems

These remain placeholders. They are intentionally not decomposed yet.

## 14.1 Game system

**Status:** Later / placeholder  
**Scale:** Large  
**Domain:** Product/runtime

### Role

The actual game/application layer consuming engine infrastructure.

### Likely dependencies

- platform system
- rendering system
- maths/geometry
- vector text/font system
- input/human interface
- threading/services
- JSON/config/state systems
- physics system
- localisation/Katakana support where needed

---

## 14.2 Editor system

**Status:** Later / placeholder  
**Scale:** Large  
**Domain:** Tooling/application

### Role

Main authoring environment for game/world/content/tool workflows.

### Likely dependencies

- platform/windowing/input
- rendering system
- vector text/font system
- image annotation/sketch layer
- JSON system
- graphics conditioning systems
- asset conditioning systems
- maths/geometry
- Host Registry/resource ownership
- file/path authority

---

## 14.3 Physics system

**Status:** Later / placeholder  
**Scale:** Large  
**Domain:** Simulation

### Role

Simulation and spatial interaction system above maths/geometry primitives.

### Likely dependencies

- vector maths
- transforms
- joints
- primitive intersection tests
- penetration tests
- threading/job services
- JSON/config/state capture
- game/editor integration

### Boundary

Maths collision/intersection primitives are not owned by physics. Physics consumes them and adds simulation policy, state, constraints, broad phase, narrow phase, and gameplay/editor-facing behaviour.

---

# 15. Broad backlog map

```text
01 Current direction
02 Completed since first interim capture
03 Active short-term backlog
04 Deferred validation backlog
05 Threading / identity / provisioning backlog
06 Foundation / bootstrap backlog
07 Host ownership / registry / permissions backlog
08 Platform / OS integration backlog
09 Build and platform validation backlog
10 Maths / geometry backlog
11 Rendering / graphics pipeline backlog
12 Vector text / fonts / image annotation backlog
13 Localisation / Katakana backlog
14 Tooling / codebase hygiene backlog
15 Higher-level consumer systems
```

---

# 16. Suggested next working passes

## Pass A: Text ingester implementation slice

Goal: define and implement the first usable low-level text ingestion path.

Likely scope:

- SuiteUTF integration boundary
- strictness options
- line-ending normalisation
- ingestion report structure
- UTF-8 validation/correction reporting
- normalised output buffer ownership

## Pass B: JSON first implementation slice

Goal: define and implement a minimal dynamic representation, parser, writer, and diagnostics path.

Likely scope:

- JSON value representation
- allocation/ownership model
- parser source view over ingested text
- writer
- line/column diagnostics
- hierarchy context strings

## Pass C: Debug support slice for text/JSON

Goal: add enough debug reporting to support parser/ingester diagnostics and invariant capture.

Likely scope:

- report payload
- severity enum
- safe output target
- allocation-at-create storage
- fatal/invariant capture path

## Pass D: Linux build setup

Goal: make Linux compilation/linking possible and begin validating cross-platform paths.

Likely scope:

- build system setup
- include/platform define fixes
- compile/link verification
- later formal threading primitive smoke/stress tests
