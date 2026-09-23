# Scope recovery: k=2 Thorup-Zwick distance oracle

## Coverage decision

A fresh live audit after ordered rooted-tree edit distance and subsequent scope
recovery merges found zero open pull requests and zero open issues on
`main@0d88d249c2d05a338e2be322c824fa8a664a9924`.

The repository already contains exact shortest paths, radix-heap Dijkstra, and
a deterministic greedy weighted graph spanner, but no distance-oracle
implementation, Thorup-Zwick branch, or previous distance-oracle PR.

This slice adds a query index with a bounded approximation guarantee rather
than another shortest-path solver or graph sparsifier.

## Public contract

`ThorupZwickK2Oracle` accepts an undirected graph with globally non-negative
`Weight` edges.

- directed graphs are rejected;
- any negative edge is rejected;
- empty graphs are valid but have no queryable vertex;
- disconnected graphs are supported;
- finite queries return a non-negative `Weight` estimate;
- queries across connected components return `nullopt`;
- query results are symmetric;
- for every finite exact shortest-path distance `d`, the estimate `D`
  satisfies `d <= D <= 3d`;
- if preprocessing or the selected detour requires a finite value outside the
  representable `Weight` domain, overflow is reported instead of saturating.

The graph itself is not retained by the oracle after preprocessing.

## k=2 hierarchy

Let `A0=V` and `A2=empty`.

For non-empty graphs define

`s = ceil(sqrt(n))`.

Each vertex is selected independently into `A1` with exact probability
`1/s` using a seeded `mt19937_64` stream. The implementation uses rejection
sampling before the residue test so the probability is not affected by modulo
bias.

To make disconnected inputs total rather than probabilistically undefined,
every connected component that received no sampled landmark gets its smallest
vertex added deterministically to `A1`.

This component-local forcing does not weaken the stretch argument. It only
ensures that every vertex has a reachable pivot.

## Pivot and bunch invariant

For every vertex `v`:

- `p(v)` is its nearest reachable landmark in `A1`;
- equal-distance pivots choose the smaller vertex id;
- `delta(v)=d(v,p(v))`;
- every reachable landmark is stored in `B(v)`;
- every reachable non-landmark `w` with
  `d(v,w) < delta(v)` is stored in `B(v)`.

Each bunch is stored in ascending vertex-id order, so membership/distance lookup
uses binary search.

Preprocessing obtains exact source distances by invoking the repository's
radix-heap Dijkstra once per source. The full all-pairs matrix is discarded;
only pivots and bunch distances remain.

## Symmetric query

For query endpoints `u,v`:

1. if `u in B(v)`, then the stored value is the exact `d(u,v)`;
2. if `v in B(u)`, likewise the exact value is available;
3. otherwise consider the detour through `p(v)`:
   `d(v,p(v)) + d(u,p(v))`;
4. symmetrically consider the detour through `p(u)`;
5. return the smaller representable candidate.

Taking both pivot directions is not needed for the classical k=2 stretch bound,
but makes the public query exactly symmetric and can only improve the estimate.

If the endpoints are in different components, neither endpoint is in the
other's bunch and neither component-local landmark is reachable from the other
endpoint, so no candidate exists and the result is `nullopt`.

## 3-stretch proof

Assume `u,v` are connected and neither direct bunch test succeeds.

In particular, `u notin B(v)`.

If `u` were a landmark, every reachable landmark would be in `B(v)`, a
contradiction. Therefore `u` is a non-landmark.

By the bunch definition,

`d(v,u) >= d(v,p(v))`.

By triangle inequality,

`d(u,p(v)) <= d(u,v) + d(v,p(v)) <= 2d(u,v)`.

Hence the pivot detour satisfies

`d(v,p(v)) + d(u,p(v)) <= 3d(u,v)`.

Every returned candidate is an actual path length, so it can never be smaller
than the exact shortest-path distance. Taking the minimum of valid candidates
therefore preserves

`d(u,v) <= D(u,v) <= 3d(u,v)`.

The proof also covers zero-distance pairs: the inequalities force the selected
detour to have length zero.

## Integer sampling

`ceil(sqrt(n))` is computed with division-based integer comparisons, avoiding
a potentially overflowing square.

For exact one-in-`s` seeded sampling, a 64-bit random draw below

`2^64 mod s`

is rejected. The remaining interval has cardinality divisible by `s`; testing
the accepted draw modulo `s` therefore gives each residue equal probability.

The seed is public diagnostic state, so two constructions from the same graph
and seed produce the same landmark/pivot/bunch structure under the repository's
standard C++ engine.

## Complexity boundary

Let `n` be the number of vertices and `m` the number of logical graph edges.

This first-principles baseline intentionally runs exact single-source shortest
paths once per vertex. Its preprocessing time is therefore

`n * T_radix_dijkstra(n,m)`

plus `O(n^2)` pivot/bunch scanning.

The oracle does not retain the all-pairs distance matrix.

Worst-case retained bunch storage is conservatively `O(n^2)`. This document
does not turn the classical expected Thorup-Zwick compactness theorem into a
stronger claim for the component-forced variant.

A query performs a constant number of binary searches in sorted bunches, giving
a conservative `O(log n)` lookup bound in the worst case.

No CI timing is used as a benchmark.

## Independent verification

Committed tests compute exact all-pairs shortest paths with a separate
Floyd-Warshall implementation. The oracle tests do not reuse production
Dijkstra, pivot selection, or bunch construction.

Coverage includes:

- empty and singleton graphs;
- directed-graph rejection;
- global negative-edge rejection;
- disconnected components with forced landmarks;
- cross-component `nullopt`;
- zero-weight edges;
- self-loops;
- parallel edges;
- deterministic same-seed landmark, pivot, bunch-size, and query behavior;
- explicit overflow rejection when exact Dijkstra arithmetic is not
  representable;
- 180 fixed-seed random graphs of 1..9 vertices;
- four independent oracle seeds per random graph;
- every ordered vertex pair on every random instance;
- exact connectivity agreement;
- query symmetry;
- `D >= d`;
- `D <= 3d`.

A supplementary model-level differential pass checked 3,000 additional random
small graphs over all vertex pairs and found zero connectivity, symmetry, or
stretch mismatch. That model pass is supporting evidence only; exact repository
GCC, Clang, and sanitizer CI remains the integration gate.

## Non-claims

This slice does not claim:

- general k Thorup-Zwick hierarchies;
- directed-graph distance oracles;
- negative-edge support;
- path reconstruction;
- dynamic edge updates;
- worst-case subquadratic storage;
- a formal expected-space theorem for the component-forced distribution;
- constant-time bunch lookup;
- benchmark-backed latency or throughput.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/graphs/thorup_zwick_k2.hpp`;
- `tests/test_thorup_zwick_k2_cases.hpp`;
- `docs/scope_recovery_thorup_zwick_k2.md`.

Pre-PR hardening includes:

- unbiased seeded landmark sampling via rejection sampling;
- explicit shortest-path overflow regression;
- synchronization of the public sampling comment with the implementation.

No CMake, README, historical ROADMAP, workflow, benchmark, compiler/backend, or
temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `0d88d249c2d05a338e2be322c824fa8a664a9924`.
