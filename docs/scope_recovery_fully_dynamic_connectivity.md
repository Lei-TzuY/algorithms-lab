# Scope recovery: online fully dynamic general connectivity

## Coverage decision

A fresh live audit after maximum-cardinality matroid union reached merged `main`
found no online fully dynamic non-forest connectivity implementation in default-
branch code, pull-request history, or branch state. The repository already has
sealed offline dynamic connectivity via segment-tree-over-time + rollback DSU and
a first-principles Euler-tour-tree dynamic **forest**. The Euler-tour-forest proof
document explicitly excludes fully dynamic non-forest connectivity.

This slice closes that gap without touching the genuinely occupied
`scope-recovery-minimum-cycle-basis` surface. It deliberately changes proof model
again after matroid union: an online spanning-forest invariant with explicit
non-tree replacement search rather than oracle reduction or offline time
segmentation.

## Production contract

`FullyDynamicConnectivity` owns a fixed dense vertex universe and supports:

- online undirected `add_edge(u,v)` / `remove_edge(u,v)`;
- exact `connected(u,v)` and component cardinality queries after every update;
- multigraph multiplicity, including parallel copies and self-loops;
- exact total/tree/non-tree edge-copy counts;
- deterministic removal semantics: a parallel non-tree copy is removed before
  the represented tree copy of the same endpoint pair;
- caller-provided Euler-tour-forest seed for replayable forest structure;
- an expensive `valid_structure()` diagnostic replaying the wrapper invariants;
- invalid vertices and inactive removals fail closed.

Self-loops are active edge copies but never affect connectivity. Edge weights are
not part of this data-structure API. Parallel copies are compressed by canonical
endpoint pair; at most one copy of a non-loop pair is represented in the spanning
forest.

## Spanning-forest invariant and replacement proof

The sealed `EulerTourForest` stores a spanning forest `F` of the active multigraph.
Every active non-loop edge therefore has endpoints connected in `F`.

Insertion has two cases. If the endpoints are in different components, the new
copy is linked into `F`. Otherwise it is recorded as a non-tree copy. Thus the
invariant is preserved.

Removing a self-loop or non-tree copy cannot change connectivity. When the only
remaining copy of a represented tree edge is removed, cutting it from `F` splits
one tree component into two parts `A` and `B`. Production then scans every active
non-tree endpoint pair. If a pair crosses `A/B`, one copy is promoted into `F`,
restoring a spanning tree of the still-connected active component.

If no non-tree pair crosses the cut, no active graph path can connect `A` to `B`:
all surviving tree edges remain wholly inside one side, and every surviving
non-tree edge was checked. Therefore the active multigraph itself is disconnected
across that cut. This proves that forest connectivity equals active-graph
connectivity after every update.

`valid_structure()` checks the sealed ETT diagnostic, exact edge-copy accounting,
one represented tree copy per marked pair, self-loop/non-tree bookkeeping, and
that every active non-loop endpoint pair is connected in the maintained forest.

## Verification

Pre-upload focused wrapper verification passed strict GCC, strict Clang, and
actual GCC ASan+UBSan against an API-compatible forest test double. Exact
integration with the sealed repository `EulerTourForest` remains a required
GitHub full-suite gate before merge.

Deterministic coverage includes:

- empty-universe and invalid-vertex contracts;
- inactive removal rejection;
- endpoint symmetry;
- duplicate parallel copies, including removal of the non-tree copy before the
  represented copy;
- self-loop multiplicity as connectivity-neutral non-tree state;
- a triangle where deletion of a represented edge must promote the remaining
  crossing non-tree edge, followed by deletion of that replacement and a real
  graph split;
- component-size and structural diagnostics.

The primary randomized oracle is structurally independent. A `std::map` stores
active edge multiplicities and ordinary rebuilt adjacency + BFS answers
connectivity/component-size queries. The focused corpus executes 6,000 fixed-seed
mixed operations over 18 vertices, checks exact edge accounting and structural
validity after every operation, and periodically compares every vertex pair plus
every component size and endpoint-pair multiplicity.

## Complexity and non-claims

Let `D` be the number of distinct active non-loop endpoint pairs. Ordered edge
bookkeeping contributes `O(log(D+1))`. Under the sealed Euler-tour forest's
random-priority treap model, connectivity/link/cut/component queries have expected
`O(log V)` time.

- insertion: expected `O(log(D+1) + log(V+1))`;
- self-loop or non-tree deletion: `O(log(D+1))`;
- represented-tree deletion: expected
  `O(log(D+1) + D log(V+1))` because this educational baseline deliberately scans
  all non-tree endpoint pairs for a replacement;
- connectivity/component-size query: expected `O(log(V+1))`;
- resident storage: `O(V + D)` plus compressed multiplicities.

The ETT treap can have a bad shape, so these are expected rather than per-operation
worst-case logarithmic bounds. This is **not** Holm-de Lichtenberg-Thorup, does
not claim polylogarithmic update time, does not maintain dynamic MST weights, and
does not expose path aggregates. `valid_structure()` is diagnostic work and is
excluded from operation bounds.

## Scope

The intended recovery checkpoint changes exactly four paths:

- `include/algorithms/data_structures/fully_dynamic_connectivity.hpp`;
- `tests/test_fully_dynamic_connectivity_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused proof document.

No CMake, README, historical ROADMAP, recovery authority, workflow, benchmark,
frozen compiler/backend, occupied minimum-cycle-basis, or temporary-file surface
is part of this slice.
