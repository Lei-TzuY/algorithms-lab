# Phase 46 sealing audit — liveness-pruned scalar SSA

Phase 46 is sealed only after the pruned-SSA implementation reached merged `main` and the exact merged-main CI matrix completed successfully.

## Integration evidence

- implementation PR: #109, `Phase 46: liveness-pruned SSA construction`;
- exact candidate: `29458348474d011bcc1cd533ba0a985a3078135f`;
- exact candidate CI run `34381169510`: GCC release, Clang release, and GCC ASan+UBSan all successful;
- integrated checkpoint: `main@3527e8f9b0f8abe9647fc6f951454c8c6b8a6f6a`;
- merged-main CI run `34381733038`: GCC release, Clang release, and GCC ASan+UBSan all successful;
- 350 fixed-seed reachable DAGs compare production liveness against an independent forward def-free path-to-use oracle and compare pruned SSA identities against an independent topological reaching-definition oracle.

## Sealed correctness boundary

The phase adds two distinct executable capabilities on the sealed scalar event IR:

1. deterministic per-variable live-in/live-out analysis on the start-reachable directed CFG;
2. liveness-conditioned dominance-frontier phi placement followed by the sealed Phase-45 version renaming and phi-incoming witness semantics.

A phi is materialized only when its variable is live-in at that block, and only a materialized phi becomes a new definition site for further frontier propagation. The implementation is therefore not minimal SSA followed by post-hoc phi filtering.

The sealed evidence includes dead-join phi elimination, retained live joins and loop-carried values, use-before-definition ordering, local kills, parallel predecessor-edge deduplication, unreachable-block semantics, repeated determinism, and exact identity-level randomized differential checks.

## Why the phase stops here

Semi-pruned SSA is intentionally not promoted: after exact liveness-pruned placement it would be a weaker intermediate regime rather than a new architectural capability. More CFG-shape micro-variants would likewise be test farming rather than a distinct proof boundary.

MemorySSA requires a memory/alias model that this scalar event IR does not yet define. Optimization passes, post-dominators/control dependence, register allocation, and incremental CFG maintenance each introduce separate state or correctness contracts and remain future hypotheses.

## Phase 47 promotion — out-of-SSA lowering

The next coherent executable boundary is SSA destruction rather than another placement variant.

Phase 47 must:

- eliminate phi nodes into predecessor-specific edge-copy bundles;
- preserve simultaneous parallel-copy semantics instead of sequentially clobbering cyclic assignments;
- split critical edges when a copy bundle cannot be placed unambiguously in either endpoint block;
- preserve sealed reachable/unreachable semantics and deduplicated predecessor identity despite parallel CFG edges;
- emit deterministic, replayable provenance connecting each inserted copy/temporary/split edge back to the SSA phi incoming value it realizes;
- verify diamonds, loop-carried swaps, critical edges, parallel predecessor edges, unreachable blocks, and fixed-seed CFGs against an independent semantic execution/renaming oracle.

Register allocation/coalescing, MemorySSA, optimization passes, post-dominators/control dependence, and incremental CFG maintenance remain outside Phase 47.
