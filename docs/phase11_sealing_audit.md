# Phase 11 sealing audit

Phase 11 is sealed after the Karger randomized global-min-cut slice reached merged `main` and the exact merged-main CI matrix completed successfully. The seal is intentionally about a new correctness contract—probabilistic output with deterministic replay—not about collecting many randomized algorithm names.

## Capability covered

- externally supplied 64-bit seed and explicit finite trial budget;
- repository-defined rejection sampling over the raw `std::mt19937_64` stream, avoiding implementation-dependent bounded-distribution mapping;
- replayable winning-trial metadata and bipartition witness;
- undirected multigraph semantics with parallel-edge multiplicity, self-loop neutrality, ignored weights for the cardinality objective, and directed-input rejection;
- deterministic exact-zero fast paths for trivial/disconnected inputs;
- a direct `O(TVE)` implementation bound rather than a Karger-Stein or other unimplemented optimization claim.

## Correctness and honesty evidence

The returned witness is always replayed against the original edge instances. An independent exhaustive anchored-bipartition oracle supplies the exact small-graph optimum. Two hundred forty fixed-seed multigraphs with 2–8 vertices and a 512-trial budget matched that oracle in the deterministic corpus.

That corpus is deliberately not promoted into an exactness theorem. A dedicated counterexample has optimum 2 while `seed=1, trials=1` reproducibly returns 3; increasing the same replay stream to three trials reaches 2 on trial 2. This executable failure-to-hit-optimum case is the phase's key anti-overclaim invariant.

The production implementation passed strict-warning GCC and Clang builds plus ASan/UBSan in both focused verification and the full repository CI. The exact merged-main run for the Karger merge completed successfully before sealing.

## Architecture assessment

Another Phase-11 slice is not justified merely to increase algorithm count. Randomized quickselect, randomized pivot variants, reservoir sampling, or similar additions would add breadth but not a comparably strong new contract boundary. The repository already has deterministic/randomized differential infrastructure; the substantial new idea added here is the distinction between replayable execution evidence and non-deterministic optimality.

## Promotion

The next substantial uncovered frontier is approximation algorithms with executable quality guarantees. Phase 12 begins with minimum vertex cover: mandatory self-loop vertices plus the endpoints of a deterministic maximal matching on the remaining graph give a constructive 2-approximation. Small graphs will be checked against exhaustive optimum covers so validity and the approximation ratio are executable, while the 2-approximation theorem remains a proof obligation rather than a test-derived universal claim.
