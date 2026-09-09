# Phase 40 — rooted represented-subtree numeric aggregation

## Public contract

`LinkCutForest::rooted_subtree_sum(root, vertex)` returns the exact sum of node values in `vertex`'s represented subtree when the containing represented tree is rooted at `root`.

- invalid dense vertex IDs throw `std::out_of_range`;
- disconnected `root` / `vertex` pairs throw `std::invalid_argument`;
- the final exact result is returned as `std::int64_t` when representable;
- `std::overflow_error` is thrown only when that final exact rooted-subtree sum is genuinely outside the `int64_t` range;
- the query may rearrange preferred paths, as all link-cut access operations do, but does not change represented topology or logical node values.

## Numeric representation invariant

Phase 39 separated auxiliary cardinality from virtual represented-child cardinality. Phase 40 adds the numeric analogue without allowing represented-path lazy tags to leak into virtual descendants.

Each node maintains three distinct exact aggregates:

1. `auxiliary_sum`: the logical values of nodes in the current auxiliary splay subtree only;
2. `virtual_sum`: the represented sums of this node's immediate virtual children only;
3. `auxiliary_virtual_sum`: the sum of `virtual_sum` over every node in this auxiliary splay subtree.

The represented sum rooted at an auxiliary node is therefore

`represented_sum = auxiliary_sum + auxiliary_virtual_sum`.

This separation is essential. A represented-path assignment or addition updates exactly the exposed auxiliary path. It changes `auxiliary_sum` and the lazy numeric state for those auxiliary nodes, but it must not scale or shift `virtual_sum` or `auxiliary_virtual_sum`, because virtual represented descendants are not on the updated path.

## Preferred/virtual transfer obligation

During `access(current)` the previous preferred right child becomes virtual and the newly preferred child stops being virtual. Phase 40 performs the same transfer for values that Phase 39 already performs for cardinalities:

- old preferred right child: add its exact `represented_sum` to `current.virtual_sum`;
- newly preferred child: subtract its exact `represented_sum` from `current.virtual_sum`;
- `pull(current)` then recomputes both auxiliary and auxiliary-virtual aggregates.

`link(first, second)` attaches the represented component rooted at `first` as a virtual child of `second`, so it adds both `represented_size(first)` and exact `represented_sum(first)` to the corresponding virtual accumulators. An exposed-edge `cut` removes the detached component through the normal auxiliary-child detach followed by `pull`.

## Rooted-subtree query

After `make_root(root)` and connectivity validation, `access(vertex)` exposes the unique root-to-vertex ancestor path as the left auxiliary side of `vertex`.

`represented_sum(vertex)` contains:

- the exposed ancestor-side contribution,
- `vertex` itself,
- every represented descendant contribution attached through the auxiliary/virtual decomposition.

The exact rooted-subtree value is therefore

`represented_sum(vertex) - represented_sum(vertex.left)`.

Only this final exact difference is narrowed to `int64_t`; intermediate represented aggregates may be wider.

## Lazy update interaction

The sealed Phase-36/37 path updates remain path-local:

- assignment replaces values on the exposed auxiliary path and updates `auxiliary_sum` from auxiliary cardinality;
- addition shifts values on the exposed auxiliary path and updates `auxiliary_sum` by `delta * auxiliary_size`;
- neither operation changes virtual numeric aggregates;
- reversal changes auxiliary order only and does not change any numeric total.

Consequently a path update can cross a vertex that owns virtual descendants without accidentally updating those descendants.

## Verification

The dedicated Phase-40 test surface uses an independent eager forest representation rather than another link-cut implementation.

Deterministic coverage includes:

- root changes and nontrivial branching;
- path assignment and path addition while virtual descendants exist;
- cut/relink and disconnected-query rejection;
- positive/negative cancellation;
- exact `INT64_MAX` overflow boundaries.

The randomized trace maintains an adjacency-matrix forest plus eager node values, computes rooted descendants by BFS/DFS, and compares exact subtree sums while interleaving topology changes, point/path assignments, path additions, path sums, path navigation, and structural invariant checks.

`valid_auxiliary_invariants()` independently reconstructs direct virtual children from path-parent relationships and verifies both `virtual_size` and exact `virtual_sum`, in addition to the sealed auxiliary/lazy/cardinality invariants.

## Complexity and claim boundary

Only constant-size exact aggregate bookkeeping is added to the existing link-cut access/rotation/link/cut machinery. The claimed bound therefore remains the standard link-cut **sequence-level amortized `O(log V)`** operation cost with `O(V)` resident storage.

This is not a worst-case `O(log V)` claim for every individual operation: one splay/access may still have linear auxiliary height, and `push_path` may use temporary space proportional to that height. CI wall-clock timing is not used as asymptotic evidence.
