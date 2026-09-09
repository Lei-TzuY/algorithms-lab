# Phase 42 — Directed Minimum Arborescence

## Scope

This phase implements an exact minimum-cost spanning arborescence rooted at a
specified vertex for a directed signed-weight multigraph. The implementation is
first-principles Chu–Liu–Edmonds: it selects minimum incoming edges, contracts
all selected directed cycles, recursively solves the contracted instance, and
expands the recursive witness back to original input edge indices.

Parallel edges are preserved. Self-loops are accepted as input but cannot be
part of an arborescence and are ignored by optimization. Ties between candidate
incoming edges are resolved deterministically by the smaller original input
edge index.

## Public contract

`chu_liu_edmonds_minimum_arborescence(vertex_count, edges, root)` returns:

- the exact optimum total cost when representable as `std::int64_t`;
- `edge_indices`, containing one selected original edge per non-root vertex;
- `incoming_edge_index[v]`, the selected original incoming edge for every
  non-root vertex, with no incoming edge for the root.

The call rejects an out-of-range root or edge endpoint. It rejects an instance
when not every vertex is reachable from the root through non-self-loop directed
edges, because no spanning rooted arborescence can then exist.

## Proof obligations and invariants

### Minimum-incoming baseline

At every recursion level, each non-root vertex selects its minimum incoming
edge under `(adjusted cost, original edge index)`. If those selected edges are
acyclic, they already form a rooted arborescence: every non-root vertex has one
incoming edge and following incoming predecessors cannot enter a directed cycle,
so the predecessor chain reaches the root.

### Cycle contraction

If selected incoming edges contain a directed cycle `C`, every feasible rooted
arborescence must break `C` by replacing at least one selected incoming edge of
`C` with an edge entering `C` from outside. Contracting `C` into one vertex and
reweighting an entering edge `(u,v)` by

`adjusted(u,v) = cost(u,v) - selected_incoming_cost(v)`

measures exactly the incremental cost of choosing that entering edge instead of
`v`'s selected incoming edge. The recursively selected entering edge therefore
identifies the exact original target vertex whose baseline incoming edge must be
replaced during expansion.

All cycles found at one level are contracted simultaneously. Recursive
contractions may therefore represent cycles of already-contracted components;
expansion uses parent-edge indices at every level so nested contractions still
reconstruct original input edges rather than only contracted endpoints.

This phase relies on the Chu–Liu–Edmonds contraction theorem as a proof
obligation. Passing tests is evidence for this implementation, not a proof of
the theorem itself.

### Exact arithmetic and representability

Adjusted costs can leave the `int64_t` range even when the final optimum is
representable. Production therefore keeps all internal adjusted costs in a
small first-principles arbitrary-precision signed integer and narrows only the
final sum of selected original edge costs. `std::overflow_error` is thrown only
when that exact final optimum total is outside the public `int64_t` range.

### Witness validity

On successful return:

- exactly `V-1` original edges are selected;
- the root has no selected incoming edge;
- every other vertex has exactly one selected incoming edge whose target is that
  vertex;
- traversing selected edges from the root reaches every vertex;
- summing the selected original costs exactly equals `total_cost`.

## Complexity

This direct educational implementation scans the current edge set to choose
incoming edges and to build each contracted graph. At least one current vertex
is eliminated by every recursive contraction level, so a conservative bound is
`O(VE)` time. The implementation materializes contracted edge vectors and
per-level bookkeeping; across recursive levels the conservative auxiliary-space
bound is also `O(VE)`. No optimized heap-based or asymptotically improved
arborescence claim is made.

## Verification

Deterministic regressions cover:

- acyclic rooted graphs;
- parallel-edge tie handling and ignored self-loops;
- one selected directed cycle;
- nested cycle contraction and exact expansion to original edge indices;
- invalid roots/endpoints and unreachable vertices;
- an internal adjusted cost outside the signed 64-bit range whose final optimum
  remains representable;
- positive and negative final-total overflow.

The fixed-seed differential suite generates 600 reachable directed multigraphs
with 2–6 vertices and signed costs in `[-10,10]`. An independent exhaustive
oracle enumerates one original incoming edge for every non-root vertex, rejects
cyclic/non-rooted selections by direct reachability, and minimizes the exact
small-instance total. Production optimum cost and reconstructed witness are
checked against that oracle.

Focused execution uses the repository warning policy under GCC and Clang plus a
real AddressSanitizer/UndefinedBehaviorSanitizer build. The exact implementation
PR #101 and the exact merged-main checkpoint `34c288d206d0d7595bf7c216289b8592ba76de04`
both passed the repository GCC release, Clang release, and GCC ASan+UBSan CI
matrix; merged-main run `34363518566` completed successfully.

## Sealed boundary

Phase 42 stops at the exact first-principles Chu-Liu-Edmonds capability above.
Alternate arborescence implementations or minor policy variants are not added
merely to increase algorithm count. The next promoted frontier is directed
control-flow dominance, whose semi-dominator/link-eval invariants and immediate-
dominator witness form a distinct proof and architecture surface.
