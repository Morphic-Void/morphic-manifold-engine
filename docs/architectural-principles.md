Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   architectural-principles.md  
Author: Ritchie Brannan  
Date:   17 Apr 26  

# architectural-principles.md

# Architectural Principles

## Overview

This codebase prefers systems with low surface area, high leverage, explicit ownership, limited dependencies, deliberate compatibility, and architectural respect for performance.
The aim is to reduce recurring friction, retain control, and place cost where it is cheapest to pay.

These principles are not style preferences.
They are intended to guide design decisions, tradeoff evaluation, maintenance, and long-term evolution.

## 1. Low surface area, high leverage

Prefer fewer mechanisms, fewer choices, fewer ownership models, fewer dependencies, and less handwritten repetition.

A smaller and sharper system is easier to reason about, easier to maintain, and easier to reuse.
The goal is not minimalism for its own sake, but more useful capability per unit of code and per unit of conceptual machinery.

## 2. Performance is an architectural concern

Treat performance as a design property, not a late optimisation pass.

Algorithmic complexity, data layout, locality, memory usage, and interface shape should be considered early enough that the system does not later require structural rescue.
Memory cost is part of execution cost.
Waste in memory becomes waste in locality, cache, and time.

## 3. Maintenance is continuous

Maintenance is a standing cost of ownership and should be paid continuously.

Technical debt compounds. Routine correction is cheaper than later rescue.
If something is known to be incorrect, it should be corrected.
Temporary bridging mechanisms and shims may be used during transition, but should not become a pretext for preserving known wrongness.

## 4. Correctness outranks attachment

Nothing is sacred except correctness.

History, familiarity, compatibility, and sunk cost do not justify preserving a wrong design or implementation.
Continuity matters, but continuity of error is not a virtue.

## 5. Small-team survivability matters

Systems should be buildable, understandable, and maintainable by as few people as are actually needed.

A codebase that depends on excessive coordination, large numbers of maintainers, or concentrated tribal knowledge carries structural cost.
The design should minimise avoidable dependence on organisational scale.

## 6. Dependencies should be minimised

Every dependency imports assumptions, coupling, fragility, and upgrade pressure.

External dependencies deserve stricter scrutiny because they also reduce control over behaviour, timing, compatibility, and long-term direction.
Prefer owned understanding over imported complexity.

## 7. Ownership should be narrow and explicit

Ownership and lifetime models should be simple, limited, and easy to reason about.

Many parts of a system may observe and use a resource. Few should own it.
Shared ownership should be exceptional because it tends to blur authority, responsibility, teardown, and mutation boundaries.

## 8. Smooth production is a design goal

Velocity depends on smoothness, and smoothness depends on limiting repeated friction.

Repeated choice is a tax. Repeated discussion is a tax. Duplicated function is a tax.
Prefer one clear way of doing common things over many arguable ways.
Deliberate early decisions can reduce future churn, inconsistency, and re-litigation.

## 9. Prefer broad compatibility over novelty

Use the lowest common feature set that genuinely delivers for the broadest practical range of platforms, toolchains, developers, and users.

Do not raise the baseline for novelty alone.
Constrained environments matter, including bare-metal and manually managed ones, but their needs should be isolated so that other targets do not pay unnecessary cost.

## 10. Use abstraction only where it reflects reality

Do not abstract for abstraction's sake, and do not force concreteness where abstraction is the right model.

Abstraction should express real shared structure.
Concrete designs should remain concrete when abstraction would only add indirection, ceremony, or speculative generality.
The level of abstraction should match the problem.

## 11. Measure, evaluate, and decide in context

Do not decide from slogans when evidence is available.

Do not assume; find out. Measure where possible.
Evaluate costs and benefits in context, including utility, placement, frequency, and lifetime.
A one-time preparation cost is not the same as a recurring runtime cost.
Human-facing forms and machine-facing forms may reasonably differ when the transformation boundary is deliberate and useful.

## Summary

These principles favour limited variance, retained control, explicit responsibility, broad practicality, and early investment where it lowers recurring cost later.
Their purpose is not to constrain thought for its own sake, but to produce systems that remain correct, efficient, maintainable, and usable over time.
