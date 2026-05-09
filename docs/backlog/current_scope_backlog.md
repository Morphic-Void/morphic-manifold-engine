
Threading support primitives:

1. DONE: Define primitive wrapper boundary
2. DONE: Implement hardware thread count query
2a. DONE: Implement a hardware thread identification query
3. DONE: Implement mutex wrapper
5. DONE: Decide whether semaphore is needed now
8. DONE: Define native thread entry contract
9. DONE: Implement native thread creation wrapper
10. DONE: Implement minimal thread start trampoline

DONE: Reorganise all the cross-platform support into the platform directories
DONE: Move the tga support into image/codec/
DONE: convert module binding to class

DONE: cleanup the platform defines
DONE: stop leakage of platform defines
DONE: use Linux path for Android wait word but add additional defines

4. IN PROGRESS - (CONSIDERING ANDROID OPTIONS): Implement wait/wake wrapper or fallback-compatible wait primitive
6. IN PROGRESS - (IT IS NEEDED AS FALLBACK): Implement semaphore wrapper if needed
7. IN PROGRESS - (AS PART OF THE FALLBACK): Implement 2-phase parking gate
11. DEFERRED UNTIL PROJECT TESTABLE ON ALL SUPPORTED PLATFORMS: Add smoke/stress tests around each layer

Random tasks:

Add the tga testing

wrapped phase gate, wait predicates and counter semaphore

add affinity, priority and numa identity

DONE: CStaticLookup for provisioning and other data uintptr_t based
TODO: - then start on the initialisation and structures