# Deterministic weighted global minimum cut

This recovery slice adds a direct Stoer-Wagner global-minimum-cut implementation
for undirected non-negative weighted multigraphs. It is intentionally distinct
from the sealed Phase-11 Karger implementation and the sealed Phase-41
Gomory-Hu cut-equivalent tree.

## Contract

`stoer_wagner_global_min_cut(vertex_count, edges)` returns `std::nullopt` when
fewer than two vertices exist, because there is no non-trivial bipartition.

For at least two vertices it returns:

- the exact minimum weighted cut as an unsigned 64-bit value;
- one deterministic non-trivial side witness of that cut;
- a normalized witness with vertex 0 on the false side.

Input uses the existing `UndirectedCapacityEdge` abstraction. Endpoints must be
in range and every signed capacity must be non-negative. Parallel edges add
their capacities. Self-loops are accepted after validation but are cut-neutral.

The direct solver is allowed to return a cut larger than the signed `Capacity`
range used by the flow APIs. Internal arithmetic therefore uses `uint64_t`.
Before construction, the implementation checked-sums every non-loop input
capacity and rejects if that total is not representable. Every later
maximum-adjacency weight and contracted edge is a non-negative sub-sum of that
same total, so this preflight is also the representability argument for every
internal addition.

## Stoer-Wagner invariant

At one phase, vertices are current contracted supernodes. Starting from an
empty set A, repeatedly add the not-yet-added supernode with maximum total edge
weight into A; vertex-id order breaks ties deterministically.

If `s` and `t` are the final two supernodes added, the phase value is the
connection weight of `t` when it is added. The Stoer-Wagner phase theorem says
that this value is a minimum `s-t` cut in the current contracted graph. After
recording the original-vertex members of `t` as a candidate global-cut witness,
`t` is contracted into `s`. Repeating until one supernode remains exposes a
phase whose cut equals the original graph's global minimum cut.

The phase theorem and contraction theorem are mathematical proof obligations.
Randomized/exhaustive tests are implementation evidence; they do not replace
those theorems.

## Determinism and witness boundary

Input edges are first aggregated into a symmetric weight matrix, so input order
does not influence phase selection. Equal maximum-adjacency choices use the
lower representative vertex id. Equal best phase cuts use a deterministic
lexicographic comparison after complement-normalizing the side to keep vertex 0
false.

This is a deterministic witness policy, not a claim that the selected side is
the globally lexicographically smallest member of the set of every optimal cut.

## Complexity

For `V` vertices, each of at most `V-1` phases performs up to `V` selections,
and each selection scans/updates `O(V)` matrix entries. The direct baseline is
therefore `O(V^3 + E)` time and `O(V^2 + E)` auxiliary/input-normalization
storage.

This is deliberately a first-principles dense baseline. It does not claim a
heap-optimized Stoer-Wagner bound or reuse max-flow as the production
implementation.

## Verification

Deterministic tests cover:

- empty and singleton inputs;
- invalid endpoints and negative capacities, including a negative self-loop;
- weighted triangles;
- parallel edges, self-loops, zero-capacity edges, and disconnected graphs;
- input-order replay determinism;
- exact internal `UINT64_MAX` total/cut representation from signed-capacity
  edges;
- fail-closed rejection when the aggregate non-loop capacity exceeds
  `UINT64_MAX`.

The randomized corpus contains 600 fixed-seed undirected multigraphs with
2-8 vertices, up to 24 edges, self-loops/parallel edges, and capacities 0-30.
For every graph:

1. a direct exhaustive bipartition oracle computes the exact global cut;
2. the returned side is replayed against the original edge list;
3. the sealed Gomory-Hu implementation is built independently and the minimum
   non-root tree-edge value is cross-checked against Stoer-Wagner;
4. the input edge order is shuffled and the complete deterministic result is
   required to remain unchanged.

The exhaustive bipartition oracle is the primary correctness oracle. Gomory-Hu
is secondary cross-implementation evidence: its repeated-Dinic construction is
structurally independent from Stoer-Wagner maximum-adjacency contraction.
