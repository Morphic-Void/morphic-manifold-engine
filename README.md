# The Manifold Engine

A C++17 game engine developed by Morphic Void. It is being built from first principles with an emphasis on clear structure, modular systems, and long-term maintainability.

This repository currently contains the initial project structure and test infrastructure. Additional systems and documentation will be added as the engine evolves.

## License

Licensed under the MIT License. See the LICENSE file for details.

Copyright (c) 2010-2026 Ritchie Brannan.

Attribution is appreciated where practical.

## Why write another engine?

The usual advice is simple: do not write your own game engine.

I have given that advice to other people, and I still think it is usually right.

Writing a game engine is not just writing a renderer, a physics system, or
a few containers. It quickly becomes a pile of interacting disciplines:
runtime architecture, memory management, asset loading, editor tooling,
build systems, input, rendering, physics, audio, platform integration,
debugging, profiling, packaging, documentation, testing, and long-term
maintenance.

Most people considering writing an engine are not just underestimating the
amount of work. They are often underestimating the number of different
skills involved, and in many cases they do not yet know enough to know which
skills will be needed. That is not a criticism; it is just the shape of the
problem. A mature engine hides a vast amount of accumulated knowledge.

I am not approaching this from the outside.

I have worked with Unreal Engine for over a decade, and I have also worked
with many non-public engines used by major publishers, most prominently at
EA and Ubisoft. That does not make writing an engine easy, but it does mean
I have direct experience of what engines actually contain: not just the
visible systems, but the accumulated tools, constraints, failure modes,
maintenance costs, production compromises, and long-tail edge cases.

For most developers, especially anyone trying to ship a first game, writing
an engine is the wrong trade. It turns one hard project into several hard
projects, and it will usually steal time from the thing that matters most:
finishing the game.

This project deliberately breaks that rule.

It does so for specific reasons, not because the rule is wrong.

First, this is partly a demonstration of what I can build. I have spent my
career working on low-level systems, rendering, optimisation, tooling, and
engine infrastructure. This codebase is a way to make some of that work
visible. It is also, unavoidably, a marketing exercise for both myself and
Morphic Void Limited: not in the sense of polished sales material, but in
the sense of showing the engineering judgement, constraints, and trade-offs
behind the work.

Second, this is what I do anyway. My sense of self is strongly tied to being
an engineer. Building and maintaining systems like this is part of how I
keep my skills sharp, test my assumptions, and continue to develop. Even if
this were not attached to a game, much of the infrastructure work would
still be work I wanted to do.

Third, the game I want to build is a poor fit for the publicly available
engines and their surrounding assumptions.

That does not mean the game needs unusually complex rendering or physics.
From my point of view, it does not. Others may disagree. The issue is not
raw complexity; it is shape. The rendering and physics requirements sit in a
place where a large general-purpose engine would likely be fighting me in
the areas where I most need direct control.

There is an obvious question: if the game is awkward to build in existing
engines, why not build a different game?

The answer is that I have been playing with this game idea for over thirty
years. I want it to see the light. If someone else had made a similar enough
game in the intervening years, this specific project might not exist in this
form, but I would almost certainly still be building something in the same
broad territory. The itch is not just one design document; it is a long-term
interest in a particular kind of game and the technology needed to support it.

The games that came closest, at least in parts of the design space that
matter to me, were *Prey* and *Portal*. They did not remove that itch.

So this repository is not only a game. It is also the engine, the general
runtime infrastructure, the pipeline infrastructure, the tooling direction,
and the game built on top of that stack. The technology is not incidental
overhead; it is part of the purpose of the project.

That still does not mean I intend to build everything from scratch.

Where existing asset pipelines, file formats, tools, libraries, or engine
components can be scavenged sensibly, I will use them. I am not trying to
prove purity, and I am not a masochist. The point is to own the parts where
ownership matters: the runtime model, the core infrastructure, the rendering
and physics decisions that shape the game, and the long-term maintainability
of the codebase.

This is not a recommendation that other people should do the same.

If your goal is simply to make and ship a game, use an existing engine
unless you have a clear, concrete, and well-understood reason not to.
The likelihood that you already have all the required skills is low.
The likelihood that you know in advance all the skills you will need
is lower still.

In this case, the cost is accepted deliberately. The engine exists because
the control, the constraints, the game idea, the engineering practice, and
the long-term infrastructure are all part of the same project.

This project is therefore not an argument against existing engines. It is an
argument for knowing what problem you are actually solving.

Most games need a finished game more than they need a new engine.

This one needs both.
