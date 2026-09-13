# Scope recovery: Sprague-Grundy impartial DAG games

## Coverage decision

A fresh post-Phase-69 live-code, open-PR, and recovery-branch audit found no
Sprague-Grundy, impartial-game, Nim-equivalence, or mex-based game-analysis
surface. The repository's `games` namespace currently contains a parity-game
solver, whose owner/priority/infinite-play semantics are a different proof model.
This slice therefore changes frontier again after the numerical
bidiagonalization checkpoint rather than extending the same matrix-decomposition
streak.

## Production contract

`algorithms::games::analyze_impartial_dag(graph)` accepts a finite **directed
acyclic** game graph under normal play. Every outgoing edge is a legal move;
edge weights and duplicate move edges are intentionally semantically irrelevant.
Undirected or cyclic input is rejected.

The result contains:

- the exact Sprague-Grundy number for every vertex;
- a deterministic winning move for each positive-Grundy position: the first
  outgoing move in graph insertion order whose target has Grundy number zero;
- the maximum Grundy number present, as a diagnostic.

`sprague_grundy_nim_sum(...)` returns the XOR of component Grundy values for a
disjoint sum.

The contract is limited to finite acyclic impartial normal-play games. It makes
no claim about loopy games, partisan games, misere play, stochastic games, or
parity-game reduction/equivalence.

## Invariant / proof obligation

Process a topological ordering in reverse. Once vertex `v` is reached, every
successor already has its final Grundy value. Production sets

`g(v) = mex({ g(w) : v -> w })`.

A terminal vertex therefore has `g(v)=0`. A position is losing exactly when its
Grundy value is zero. If `g(v)>0`, then zero belongs to the reachable set by the
mex definition, so at least one move to a zero-Grundy position exists; the
returned witness chooses the first such graph edge deterministically.

For independent game components, the Sprague-Grundy theorem states that the
disjoint sum is equivalent to a Nim heap whose size is the XOR of component
Grundy values. This theorem is a proof obligation; randomized tests are evidence
for the implementation, not a replacement for it.

## Independent verification

Focused repo-native verification passes strict GCC C++20, strict Clang C++20,
and real GCC ASan+UBSan builds.

The primary exact-value oracle deliberately does **not** recompute mex. For 350
fixed-seed random DAGs with 1..10 vertices, each game position is combined with
Nim heaps `k=0..n`. A direct recursive normal-play solver on product states
`(position, heap)` finds the unique `k` for which the sum is losing; that `k`
must equal the production Grundy value.

A second independent check generates 180 pairs of random DAG games and directly
solves every two-component product state `(u,v)` by recursive win/lose search.
Its result must agree with `g(u) XOR g(v) != 0` for every state pair.

Deterministic cases cover empty games, terminals, chain/fork mex values,
deterministic winning-move selection, duplicate weighted edges, arbitrary ignored
weights, cycles/self-loops, undirected rejection, and direct Nim-XOR examples.

## Complexity / storage boundary

Kahn topological sorting is `O(V+E)`. Reverse-order mex processing allocates a
marker array of size `outdegree(v)+1` per processed vertex, so total marker
initialization and edge scanning remain `O(V+E)` with `O(V+E)` resident graph plus
`O(V + max_outdegree)` analysis workspace/result state. No loopy-game fixed point
or general game-graph solver is implied.

## Scope

The recovery slice is intentionally narrow: one header-only implementation, one
native test header, one include in `tests/test_main.cpp`, and this proof record.
It does not modify CMake, README, historical ROADMAP, recovery authority,
workflows, benchmarks, numerical code, or frozen compiler/backend surfaces.
