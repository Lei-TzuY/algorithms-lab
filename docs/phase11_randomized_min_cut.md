# Phase 11 — randomized global minimum cut

Phase 11 introduces algorithms whose executable contract includes randomness. The first slice is Karger's contraction algorithm for the cardinality global edge cut of an undirected multigraph. The API deliberately reports the **best cut observed within a finite trial budget**; it does not relabel a Monte Carlo search as a deterministic exact solver.

## Contract

`karger_randomized_global_min_cut(graph, seed, trials)` accepts an undirected `Graph` and returns:

- `best_cut_size`: the smallest cut cardinality observed by the executed trials;
- `side[v]`: a replayable bipartition witness for that observed cut;
- the input `seed` and `requested_trials`;
- `trials_executed`, which is zero for deterministic trivial/disconnected fast paths and otherwise equals `requested_trials`;
- `winning_trial`, the zero-based trial that first produced the returned best cut, or no value when no randomized trial was required.

At least one trial must be requested. Directed input is rejected. Edge weights are intentionally ignored because this slice minimizes **edge cardinality**, not weighted capacity. Parallel edges retain multiplicity and therefore affect both contraction probability and cut cardinality. Self-loops are ignored.

Graphs with at most one vertex have global cut zero. A disconnected graph also has exact global cut zero; the implementation detects this deterministically and returns the component containing vertex zero as a zero-cut witness without consuming randomized trials.

## Replay semantics

The contraction stream is derived from `std::mt19937_64(seed)`, but bounded edge selection does not use `std::uniform_int_distribution`. The implementation maps raw 64-bit engine output to a candidate index with explicit rejection sampling. That keeps the current seed-to-contraction mapping under repository control rather than depending on a standard-library distribution implementation.

For the same graph insertion order, implementation version, seed, and trial budget, the returned witness and winning-trial metadata are deterministic. Changing the graph insertion order changes the edge-instance order and can therefore change the replay stream while preserving the randomized algorithm's semantics.

## Contraction invariant

Each trial starts with one super-vertex per original vertex. While more than two super-vertices remain:

1. enumerate original non-loop edge instances whose endpoints currently belong to different super-vertices;
2. choose one such crossing edge uniformly through the explicit rejection sampler;
3. contract its endpoint components with a local union-by-size structure.

Parallel original edges remain separate entries, so a pair of super-vertices connected by multiple edge instances has proportionally greater contraction probability. Once two super-vertices remain, the cut witness is the partition induced by the component containing vertex zero, and the cut cardinality is replayed over the original edge instances.

A connected graph with more than one vertex must always expose at least one crossing candidate until two components remain. An empty candidate set before that point is treated as an internal invariant failure rather than silently producing a result.

## Complexity boundary

The current implementation rebuilds the list of currently crossing original edge instances before every contraction. A connected trial performs at most `V - 2` contractions, each scanning `E` edge instances, and then replays the final cut over `E` edges. The direct-code bound is therefore `O(VE)` time per trial and `O(TVE)` for `T` requested trials, with `O(V + E)` auxiliary storage.

No faster Karger/Karger-Stein bound is claimed because this implementation does not contain those optimizations.

## Probabilistic boundary

This API is Monte Carlo. A finite connected-graph run returns a valid observed cut but does **not** certify that the cut is globally minimum. The repository therefore makes no numeric success-probability guarantee for a chosen trial count in the public contract. Textbook probability theorems for ideal random contraction are mathematical background, not a test-derived exactness certificate for one finite execution.

The non-exact boundary is executable, not just documentary. A fixed six-vertex graph in the regression suite has exact minimum cut 2; with seed 1 and one trial the implementation reproducibly observes cut 3, while the same seed with three trials observes cut 2 at trial 2.

## Verification

Deterministic coverage checks directed-input and zero-trial rejection, empty/singleton behavior, disconnected exact-zero fast paths, self-loop neutrality, parallel-edge multiplicity, ignored weights, fixed-seed replay, and the explicit one-trial suboptimal counterexample.

The independent exact oracle exhaustively enumerates anchored bipartitions for small graphs and counts original edge multiplicity directly. Two hundred forty fixed-seed random multigraphs with 2–8 vertices are each run with a 512-trial budget; for this deterministic test corpus the best observed Karger cut must equal the exhaustive optimum, and every returned witness is replayed independently. This is implementation evidence for the selected corpus, not a claim that 512 trials make arbitrary inputs exact.

## Frontier

Karger contraction establishes the Phase-11 contract discipline: randomness is externally seedable, trial budgets are explicit, witnesses are replayable, and probabilistic output is not mislabeled exact. Phase 11 remains active after this slice; a later architecture audit should decide whether another genuinely distinct randomized paradigm is worth adding before sealing the phase.
