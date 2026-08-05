Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
License: MIT (see LICENSE file in repository root)

File:   attribution_policy.md
Author: Ritchie Brannan
Drafting and editorial assistance: OpenAI tools
Date:   5 Aug 26

# Attribution Policy

## Purpose

This document explains how authorship and AI assistance are represented in this
repository.

The project uses a mixed human/AI workflow. That workflow is not uniform across
all files, and attribution is intended to reflect the source of substantive
content rather than only who typed the final text.

## Project ownership and decision authority

Project direction, specification authority, architectural decisions, and final
acceptance remain with Ritchie Brannan / Morphic Void Limited.

AI tools may assist with drafting, refactoring, review, implementation,
testing, summarisation, and documentation, but they do not replace project
ownership or final technical authority.

## Accountable adoption

If code or documentation is retained in this repository, it is here by the
decision and authority of Ritchie Brannan / Morphic Void Limited.

Differences in origin matter for attribution, but they do not dilute
responsibility for what is adopted and retained in the framework.

This should not be read as a public warranty or a guarantee of fitness for
purpose.

## General attribution rule

File attribution in this repository is based primarily on substantive origin
and responsibility, not on keystroke origin alone.

This means a file may still be attributed to Ritchie Brannan where the
underlying design, structure, constraints, and intended result were specified
in sufficient detail that AI assistance functioned mainly as drafting,
transcription, refactoring, or implementation support.

Conversely, where AI tooling contributed most of the concrete structure,
exposition, or implementation detail beyond a relatively loose brief, the file
may be marked as primarily AI-drafted or AI-implemented under human review and
acceptance.

## Practical categories

### 1. Human-authored

Use this where the file is substantively the work of Ritchie Brannan, including
cases where AI assistance was used for review, wording, refactoring, or
implementation against a sufficiently strong human specification.

Typical header:

- `Author: Ritchie Brannan`

Ordinary AI assistance does not require additional per-file annotation in this
category.

### 2. Human-directed collaborative

Use this where the substantive result emerged through significant iteration
between Ritchie Brannan and AI tools, with both human direction and AI
contribution materially shaping the outcome.

Typical header:

- `Author: Ritchie Brannan with OpenAI tools`

This category should be used selectively where a simple single-author label
would materially misrepresent the file.

### 3. AI-primary under human review

Use this where AI tools produced most of the concrete structure, exposition, or
implementation detail, and the human role was primarily direction, review,
correction, and acceptance.

Typical header:

- `Primary draft: OpenAI tools`
- `Reviewed and accepted by: Ritchie Brannan`

This category is expected to apply more often to selected documentation, test
harnesses, or support files than to the main body of the production code.

Where the human role is primarily practical use rather than close authorship
review, a suitable secondary line may be:

- `Used, occasionally adjusted, and accepted by: Ritchie Brannan`

## Documentation-specific guidance

Different documentation classes may deserve different treatment.

High-level project documents such as ethos, principles, backlog, roadmap,
origins, and similar project-direction artifacts should usually be treated as
human-authored where they primarily express Ritchie Brannan's thinking,
priorities, design direction, or engineering judgment, even if AI tools helped
aggregate notes, improve structure, or polish wording.

Technical background, audits, milestone summaries, and subsystem reference
documents may fall into either the collaborative or AI-primary categories when
AI tools contributed a substantial share of the concrete explanatory content.

## Why this policy exists

The repository is expected to continue shifting toward a workflow in which
Ritchie Brannan increasingly operates in design, specification, review, and
acceptance roles while AI tools assist more heavily with drafting and some
implementation work.

This policy exists so that attribution remains consistent and honest under that
workflow, without forcing noisy or low-value annotations onto every mixed file.

## Default preference

When in doubt:

- keep `Author: Ritchie Brannan` on files that are substantively his work;
- avoid adding routine AI-assistance notes to files where they add little
  clarity;
- reserve explicit AI-primary labels for files where not doing so would be
  materially misleading.
