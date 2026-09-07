# Phase 7 — selected advanced integration: heavy-light decomposition

The final ordered Phase-7 implementation slice is intentionally an integration algorithm rather than another isolated textbook routine. It combines the existing undirected `Graph` abstraction with the Phase-4 checked `SegmentTree` to support mutable vertex weights, path sums, and rooted-subtree sums.

## Input and validation contract

`HeavyLightDecomposition` accepts a non-empty undirected graph, one signed `int64_t` value per vertex, and an explicit root. Construction rejects directed graphs, self-loops, parallel edges, cycles, disconnected input, a value-count mismatch, and an invalid root before query state is exposed. Graph edge weights are ignored; the maintained values belong to vertices.

The strict tree contract deliberately matches the repository's LCA/tree-DP expectations. The current implementation validates independently rather than sharing an internal rooted-tree index; consolidating that duplicated validation is a possible later architecture cleanup, not a reason to weaken this slice's correctness boundary.

## Heavy-light invariants

Construction roots the tree iteratively and records `parent`, `depth`, and a traversal order. Reverse traversal computes exact subtree sizes. Each non-leaf chooses the child with greatest subtree size as its heavy child, breaking equal-size ties by the smaller vertex ID.

The decomposition walks heavy chains first and defers light-child subtrees. This establishes two key layout properties:

- every heavy chain occupies one contiguous interval in the linear position space;
- every rooted subtree occupies `[position[v], position[v] + subtree_size[v])`.

Because a light edge goes to a child whose subtree is at most half the current subtree size, any root-to-vertex route crosses `O(log V)` light edges and therefore `O(log V)` heavy chains.

## Segment-tree integration

Vertex values are reordered into HLD position order and materialized in the existing Phase-4 `SegmentTree`.

- `assign(v, value)` delegates to transactional point assignment and is `O(log V)`.
- `subtree_sum(v)` is one contiguous segment-tree range query and is `O(log V)`.
- `path_sum(u, v)` repeatedly consumes the deeper chain-head interval until both vertices share a chain, then consumes their final in-chain interval. At most `O(log V)` ranges are queried, each in `O(log V)`, so the total is `O(log^2 V)`.

Cross-chain accumulation uses checked `int64_t` addition. A mathematically unrepresentable path or subtree sum throws rather than wrapping. Construction and updates also inherit the SegmentTree representability contract: every stored linear-tree node summary must remain representable, and a rejected point assignment leaves the structure unchanged.

Preprocessing is `O(V + E)` before the linear segment-tree build, which is `O(V)` for a valid tree; total auxiliary/state usage is `O(V)`.

## Verification

Deterministic regressions cover path/subtree semantics, point updates, rerooted subtree interpretation, singleton input, invalid vertices, all strict-tree rejection cases, constructor representability failure, transactional update overflow, and a cross-chain path whose final mathematical sum exceeds `int64_t`.

Fixed-seed randomized verification generates 300 trees with 1–45 vertices. Each tree chooses a random root and executes 120 mixed point updates, path queries, and subtree queries. Production results are compared with an independent test-only oracle that rebuilds path parents with BFS and classifies rooted descendants directly from a naïve parent array. The oracle does not reuse HLD chains, positions, subtree intervals, or the SegmentTree.

Focused pre-upload checks passed under GCC and Clang with the repository strict-warning flags, plus GCC AddressSanitizer + UndefinedBehaviorSanitizer.

## Frontier

Computational geometry, uint64 number-theory foundations, and this Graph-to-SegmentTree HLD integration now cover all ordered Phase-7 implementation entries. Phase 7 is implementation complete but not sealed. The required next step is an architecture/integration audit followed by merged-main evidence; only a clean audit may seal the phase and choose the next architectural frontier.
