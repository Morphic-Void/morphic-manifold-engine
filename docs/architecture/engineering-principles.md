Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited  
License: MIT (see LICENSE file in repository root)  

File:   engineering-principles.md  
Author: Ritchie Brannan  
Date:   17 Apr 26  

# engineering-principles.md

## Principles

- Reduce surface area; increase leverage.
- Treat performance and memory use as design concerns.
- Pay maintenance continuously; do not preserve known wrongness.
- Build for the smallest sufficient ownership and maintenance footprint.
- Minimise dependencies, especially external ones.
- Keep ownership simple, narrow, and explicit.
- Reduce repeated choice and duplicated function.
- Prefer broad compatibility over novelty.
- Abstract only where it reflects real structure.
- Measure and evaluate; do not reason from slogans.

## Engineering Principles

Build systems with low surface area and high leverage.
Prefer fewer mechanisms, fewer choices, fewer ownership models, fewer dependencies, and less handwritten repetition.
Seek more capability, more reuse, more consistency, and more useful work per unit of code.

Treat performance as an architectural property, not a late optimisation pass.
Choose algorithms, data layouts, memory usage, and interfaces with realistic scale in mind.
Avoid structures that are comfortable at small scale but require major refactoring to survive real use.
Memory efficiency is part of execution efficiency; wasted memory becomes wasted locality, cache, and time.

Pay maintenance continuously.
Technical debt compounds, so routine correction is cheaper than later rescue.
If something is known to be wrong, correct it. Nothing is sacred except correctness.
Compatibility, history, and attachment may justify transition mechanisms,
but not preservation of known error.

Design for small-team survivability.
Systems should be buildable, understandable, and maintainable by as few people as are actually needed.
Excess coordination cost is itself a design failure.

Minimise dependencies, and be stricter still with external ones.
Every dependency imports assumptions, coupling, fragility, and upgrade pressure.
External dependencies additionally reduce control over behaviour, timing, and long-term direction.
Prefer owned understanding over imported complexity.

Keep ownership narrow, explicit, and simple.
Many parts of a system may observe and use; few should own.
Shared ownership should be exceptional, because it tends to blur authority, responsibility,
teardown, and mutation boundaries.

Optimise for smooth production, not local convenience. Repeated choice is a tax.
Repeated discussion is a tax. Duplicated function is a tax.
Prefer one clear way of doing common things over many arguable ways.
Make deliberate decisions early when doing so removes future friction, inconsistency, and re-litigation.

Prefer the broadest viable foundation over novelty. Use the lowest common feature set that genuinely
delivers for the broadest practical range of platforms, toolchains, developers, and users. Do not raise
the baseline for novelty alone. Constrained environments matter, including bare-metal or manually
managed ones, but their needs should be isolated so other targets do not pay unnecessary cost.

Use abstraction where it expresses real shared structure, and avoid it where it only adds indirection
or ceremony. Do not abstract for abstraction's sake, and do not force concreteness where abstraction
is the right model. The level of abstraction should match the problem, not an aesthetic preference.

Do not decide from slogans when evidence is available. Do not assume; find out. Measure, evaluate,
and judge in context. Costs are only meaningful relative to utility, placement, frequency, and lifetime.
A one-time preparation cost is not the same as a recurring runtime cost. Human-facing forms and
machine-facing forms may differ when the transformation boundary is deliberate and useful.

## TL;DR

Prefer systems with low surface area, high leverage, explicit ownership, limited dependencies,
deliberate compatibility, and architectural respect for performance. Reduce recurring friction,
correct known wrongness, and make decisions from measured cost and actual utility rather
than fashion, slogans, or attachment.
