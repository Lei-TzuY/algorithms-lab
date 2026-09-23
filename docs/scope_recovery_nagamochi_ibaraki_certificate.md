# Scope recovery: Nagamochi-Ibaraki sparse cut certificate

## Coverage decision

A fresh live-state audit at
`main@ccd7cf57cf6e95352f35baaa6b00470a26ac60ee` found zero open pull
requests and zero open issues.

Several initially considered frontiers were rejected rather than duplicated:

- rANS byte coding already had an occupied recovery branch;
- exact Steiner tree, minimum cycle basis, treewidth, Robinson-Schensted, and
  top-trading-cycles were already implemented or occupied;
- Bostan-Mori was rejected after reading the existing Berlekamp-Massey slice,
  because the repository already exposes an `O(k^2 log n)` arbitrary nth-term
  linear-recurrence evaluator, so another nth-term engine would have been an
  algorithm substitution rather than a new executable contract.

No Nagamochi-Ibaraki / sparse k-edge-connectivity certificate implementation,
branch, or pull-request history was present.

This slice therefore adds a new graph-compression capability rather than another
minimum-cut solver: construct a sparse subgraph that preserves every cut up to a
chosen threshold.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend frontier is
not resumed.

## Public contract

`nagamochi_ibaraki_sparse_certificate(vertex_count, edges, threshold)`
accepts an undirected multigraph represented by insertion-ordered endpoint
pairs.

- each input position is a stable edge id;
- parallel edges are distinct;
- self-loops are valid input but are never selected, because they cross no cut;
- every endpoint must be in `[0, vertex_count)`;
- invalid endpoints are rejected before construction;
- `threshold == 0` returns the empty certificate after validation;
- the result is a deterministic list of original edge ids.

The returned ids define a subgraph `H` of the input graph `G`.

For every vertex subset `S`:

`|delta_H(S)| >= min(threshold, |delta_G(S)|)`.

Because `H` is a subgraph of `G`, also
`|delta_H(S)| <= |delta_G(S)|`.

Therefore every cut smaller than the threshold is preserved exactly, while
every larger cut retains at least `threshold` crossing edges.

## Construction

Let `G_0 = G` after ignoring self-loops, which are irrelevant to cuts.

For rounds `i = 1, 2, ...`:

1. build a deterministic spanning forest `F_i` of the current remaining graph
   by scanning original edge ids in increasing order and using disjoint-set
   union;
2. append the selected original edge ids to the certificate;
3. remove `F_i` from the remaining graph;
4. stop after `threshold` rounds or when no forest edge remains.

The implementation caps the number of attempted rounds by the number of
non-loop input edges, so an enormous threshold cannot create empty work after
all selectable edges are exhausted.

This is the repeated-spanning-forest form of the Nagamochi-Ibaraki sparse
k-certificate. This slice does **not** claim to implement the separate
maximum-adjacency-ordering presentation.

## Cut-preservation proof

Fix an arbitrary cut `S`, and suppose the remaining graph at the beginning of
one round still has at least one edge crossing `S`.

That crossing edge belongs to a connected component containing vertices on both
sides of the cut. Any spanning tree of that connected component must itself
contain at least one edge crossing `S`; otherwise its vertices on opposite
sides could not be connected.

Therefore the spanning forest selected in that round removes at least one
remaining crossing edge of `S`.

If the original cut contains `t` crossing edges, then after
`min(threshold, t)` nonempty rounds the certificate has selected at least
`min(threshold, t)` of them.

This argument is simultaneous for every cut because each forest spans every
connected component of the same remaining graph.

Since certificate edges are never invented, a cut with `t < threshold`
cannot contain more than `t` certificate edges. Combined with the lower bound,
all `t` original cut edges must therefore be present.

## Local edge-connectivity consequence

For vertices `u` and `v`, local edge connectivity is the minimum cut
separating them.

The cut guarantee implies

`min(threshold, lambda_H(u,v)) =
  min(threshold, lambda_G(u,v))`.

Thus connectivity below the threshold is preserved exactly, and connectivity at
or above the threshold remains certified as at least the threshold.

No global minimum-cut value is computed by this API.

## Sparsity and complexity

Each spanning forest contains at most `vertex_count - 1` edges.

Hence the certificate contains at most

`min(|E_nonloop|, threshold * (vertex_count - 1))`

edges when the multiplication is representable. The implementation never needs
to form that product.

Let `m` be the input edge count and
`r = min(threshold, |E_nonloop|)`.

Each round scans the input edge array once and performs disjoint-set operations,
so the direct implementation has conservative time bound

`O(m + r * m * alpha(n))`

and auxiliary storage `O(n + m)`.

No linear-time Nagamochi-Ibaraki implementation, maximum-adjacency ordering,
dynamic update bound, external-memory bound, or benchmark-backed performance
claim is made.

## Independent verification

Committed tests do not infer correctness from the construction.

For every tested graph, they reconstruct the certificate from returned original
edge ids and independently enumerate **every nontrivial vertex cut**. For each
cut they verify:

- certificate cut size never exceeds the original cut size;
- certificate cut size is at least
  `min(threshold, original_cut_size)`;
- cuts smaller than the threshold are preserved exactly.

Committed deterministic coverage includes:

- zero threshold;
- legal empty graph;
- one-vertex self-loop-only graph;
- invalid endpoint rejection even when threshold is zero;
- a triangle at thresholds 0 through 3;
- parallel-edge identity with self-loops;
- deterministic original-edge-id selection;
- disconnected multigraphs;
- 420 fixed-seed random multigraphs with 1..8 vertices and 0..23 edges;
- thresholds 0 through 4 for every random graph;
- exhaustive enumeration of all nontrivial cuts for every random instance;
- duplicate selected-id rejection;
- the `threshold * (n-1)` sparsity bound when representable.

A supplementary model-level stress pass evaluated 120,000 additional
`(graph, threshold)` configurations with exhaustive cut checking and found zero
violation. That model evidence is supporting evidence only; repository
GCC/Clang/sanitizer CI remains the integration gate.

## Non-claims

This slice does not claim:

- weighted cut preservation;
- spectral sparsification;
- cut-value approximation above the threshold;
- dynamic insertion/deletion support;
- maximum-adjacency ordering;
- minimum-cut computation;
- optimal construction time;
- benchmark-backed throughput.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/graphs/nagamochi_ibaraki_certificate.hpp`;
- `tests/test_nagamochi_ibaraki_certificate_cases.hpp`;
- `docs/scope_recovery_nagamochi_ibaraki_certificate.md`.

Pre-PR verification caught and fixed one test-oracle defect: the sparsity-bound
check divided by `vertex_count - 1` for a one-vertex graph. The production
algorithm was unchanged; the verifier now handles zero/one-vertex graphs
explicitly.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `ccd7cf57cf6e95352f35baaa6b00470a26ac60ee`.
