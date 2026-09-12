# Scope recovery: Yen k-shortest loopless paths

## Coverage decision

A fresh post-Phase-69 live audit at `main@9b12ce6c` found no Yen / k-shortest
loopless-path capability in repository code, pull-request history, or branches.
The current recovery stream already has single-source shortest paths, Johnson
APSP, and many graph capabilities, but no ordered enumeration of multiple
simple source-target path witnesses. This slice changes the proof obligation
from one optimum path to ranked distinct-path enumeration.

The same audit deliberately avoids currently occupied recovery surfaces named by
the preceding weighted-matroid slice: fractional cascading, minimum cycle basis,
general graph isomorphism, and push-relabel. The frozen Phase-45--69 compiler /
backend surface remains untouched. Prospective authority remains
`docs/scope_recovery_after_phase69.md` plus fresh live-state coverage audits.

## Production contract

`yen_k_shortest_loopless_paths(graph, source, target, k)` accepts a directed
`Graph` with globally non-negative `int64_t` edge weights and returns at most
`k` distinct loopless source-target paths in nondecreasing total weight.

The witness contains both vertices and exact adjacency-edge references
`(from, adjacency_index)`. Parallel adjacency entries are therefore distinct
logical choices even when endpoint and weight match. Self-loops are accepted by
the graph abstraction but cannot appear in a loopless witness. Undirected input
is rejected because the current `Graph` abstraction does not expose a stable
logical identity tying the two stored directions of one undirected edge.

The function validates source/target and rejects any negative edge anywhere in
the graph, matching the repository Dijkstra contract. `k == 0` returns an empty
result after validation. `source == target` has exactly one zero-edge loopless
path. Unreachable targets return an empty vector. Checked signed addition fails
closed if a required finite path cost is not representable in `Weight`.

## Yen invariant

Let `A` be the accepted ranked paths and `B` the persistent candidate heap.
After accepting path `A[r-1]`, production considers every spur position on that
path. Its root prefix is fixed; every earlier root vertex is temporarily removed
to preserve looplessness. For every already accepted path sharing that exact
edge-prefix, the next adjacency edge is temporarily removed, forcing the spur
search to deviate from all accepted paths with the same root.

A deterministic Dijkstra search finds the minimum-cost remaining spur path.
Concatenating the fixed root and spur yields a valid loopless candidate. Duplicate
edge sequences are suppressed across both `A` and `B`. The minimum-weight
candidate in persistent `B` is the next accepted path. This is the standard Yen
candidate-partition argument: every not-yet-accepted loopless path diverges from
some accepted path at a spur position represented in `B`, so extracting the
minimum candidate advances the global path-cost ranking.

Ties are deterministic under stable adjacency order and internal edge-reference
tie breaking, but no canonical global lexicographic ordering among equal-cost
paths is claimed. The public ordering guarantee is nondecreasing total weight.

## Verification

Focused repo-style verification passes strict GCC, strict Clang, and actual GCC
ASan+UBSan, 5/5 each.

Deterministic cases cover:

- directed-input / source-target / global-negative-weight validation;
- `k == 0`, unreachable targets, and `source == target`;
- parallel edges as distinct path witnesses;
- several equal-cost loopless paths and deterministic replay;
- checked path-cost overflow.

The primary oracle independently enumerates every simple source-target path by
DFS over exact adjacency-edge references, sorts only by exact total weight, and
compares the production prefix of the ranking by cost. Five hundred fixed-seed
random directed multigraphs use 2--7 vertices, zero/nonzero weights, self-loops,
and occasional parallel edges; each trial requests 1--12 paths. Every returned
witness is replayed against the original graph, checked for distinct vertices,
exact edge identity, and exact total cost.

Testing is implementation evidence, not a proof of Yen's theorem or of Dijkstra's
settled-distance theorem.

## Complexity and non-claims

For `K` requested paths, `V` vertices, and `E` stored directed edges, this direct
educational implementation may run one exclusion-aware Dijkstra search per spur
position of each accepted path. Candidate paths are materialized explicitly.
A conservative bound is

`O(K V (E log V + K V))` time and `O(E + K V^2)` auxiliary/result storage.

The slice does not claim Eppstein's asymptotic bound, negative-edge support,
canonical ordering among equal-cost paths, undirected logical-edge identity,
implicit-path compression, or a path-count completeness guarantee beyond the
finite set of loopless source-target paths.

## Scope

The intended recovery PR changes exactly four paths: one header-only production
implementation, one repo-native recovery test header, one include line in
`tests/test_main.cpp`, and this focused proof document. No CMake, README,
historical ROADMAP, recovery authority, workflow, benchmark, frozen
compiler/backend, occupied-recovery-surface, or temporary-file churn belongs in
this slice.
