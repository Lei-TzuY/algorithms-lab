# Scope recovery — Gallai–Edmonds decomposition

## Coverage decision

After bounded exact DAG linear-extension counting reached its merge gate, a fresh
live code / pull-request / branch audit found no Gallai–Edmonds, factor-critical,
or equivalent general-matching decomposition capability. The repository already
contains Edmonds blossom maximum-cardinality matching and the bipartite
Dulmage–Mendelsohn decomposition; this slice therefore adds the missing general-
graph structural decomposition instead of another matching solver or another
subset-DP counting routine.

The historical Phase-45–69 compiler/backend surface remains prospectively frozen.
`docs/scope_recovery_after_phase69.md` plus fresh live coverage remains the
authority for this recovery work.

## Production contract

`gallai_edmonds_decomposition(const Graph&)` accepts an undirected multigraph and
returns:

- one sealed Edmonds-blossom maximum-matching witness;
- the standard sorted Gallai–Edmonds vertex sets `D`, `A`, and `C`;
- deterministic connected components of the induced graphs `G[D]` and `G[C]`;
- the exact number of blossom-solver calls made by this direct baseline.

Ordinary matching semantics are inherited from the sealed blossom surface:
self-loops are ignored, parallel copies collapse to one endpoint relation, and
stored weights are irrelevant. Directed input is rejected. The decomposition
sets/components are canonical for dense vertex ids; the returned maximum matching
is deterministic only to the extent guaranteed by the sealed blossom routine and
is not claimed to be canonical among all optima.

## Characterization and proof boundary

Let `nu(G)` denote maximum matching cardinality. A vertex `v` belongs to the
Gallai–Edmonds set `D` exactly when some maximum matching leaves `v` exposed.
Production uses the equivalent deletion characterization

`v in D  <=>  nu(G-v) = nu(G)`.

The forward direction deletes an already exposed vertex from a maximum matching.
For the reverse direction, any matching of `G-v` with cardinality `nu(G)` is also
a maximum matching of `G` and leaves `v` exposed. Production therefore computes
one maximum matching of `G`, repeats the sealed blossom solver once for every
single-vertex deletion, and marks exactly those vertices preserving the optimum
cardinality.

It then defines

- `A = N(D) \\ D`, using the underlying simple endpoint adjacency; and
- `C = V \\ (D union A)`.

Connected components are materialized independently inside `D` and `C`.

The classical Gallai–Edmonds structure theorem supplies the deeper obligations:
every component of `G[D]` is factor-critical, `G[C]` has a perfect matching, and
every maximum matching pairs each vertex of `A` into a distinct component of
`D` while using near-perfect matchings inside the remaining `D` components.
Those statements are mathematical proof obligations. The test suite replays them
as implementation evidence; finite testing is not presented as a proof of the
theorem.

## Verification

Focused repo-style verification passes strict GCC, strict Clang, and actual GCC
ASan+UBSan.

Deterministic cases cover empty/singleton input, a graph with a perfect matching,
odd cycles, a star exposing the `D/A` split, disconnected mixtures, self-loops,
parallel copies with differing signed weights, directed rejection, and repeat
execution.

The primary randomized oracle is structurally independent from production. For
700 fixed-seed undirected multigraphs with `0..8` vertices it enumerates **every
matching**, extracts the exact maximum cardinality, marks each vertex that is
unmatched in at least one maximum matching, and derives `D`, `A`, and `C`
directly from that set. Production must match all four quantities exactly.

The same exhaustive engine additionally checks the theorem witnesses without
calling blossom: deleting each vertex from every reported `D` component leaves a
perfect-matchable remainder, every `C` component is perfectly matchable, and the
returned production maximum matching pairs `A` into distinct `D` components while
matching `C` internally.

## Complexity and non-claims

The direct implementation performs one blossom call on `G` and one on every
`G-v`. It also materializes a dense simple adjacency matrix and each reduced
graph directly. Using the sealed blossom surface's conservative `O(V^2 E)` bound,
this baseline is conservatively `O(V^3 E + V^3)` time and `O(V^2 + E)` peak
working/result storage (excluding transient storage owned inside one blossom
call).

No linear-time Gallai–Edmonds algorithm, canonical maximum matching, weighted or
b-matching decomposition, dynamic updates, factor-critical ear witness, or new
matching asymptotic claim is made.

## Scope

The intended recovery checkpoint changes exactly four paths:

- `include/algorithms/graphs/gallai_edmonds.hpp`;
- `tests/test_gallai_edmonds_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused proof/coverage document.

No CMake, README, stale historical ROADMAP, recovery-authority, workflow,
benchmark, frozen compiler/backend, or unrelated recovery surface is changed.
