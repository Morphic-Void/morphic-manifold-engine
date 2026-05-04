
Threading support primitives:

1. DONE: Define primitive wrapper boundary
2. DONE: Implement hardware thread count query
2a. DONE: Implement a hardware thread identification query
3. DONE: Implement mutex wrapper
4. IN PROGRESS: Implement wait/wake wrapper or fallback-compatible wait primitive
5. Decide whether semaphore is needed now
6. Implement semaphore wrapper if needed
7. Implement 2-phase parking gate
8. Define native thread entry contract
9. Implement native thread creation wrapper
10. Implement minimal thread start trampoline
11. Add smoke/stress tests around each layer