# Phase 39 — rooted virtual-subtree accounting

Phase 39 extends the sealed link-cut forest with exact rooted represented-subtree cardinality. The implementation does not retain a duplicate explicit forest and does not answer the query by scanning all vertices. Instead it maintains represented-size contributions from virtual children while the preferred-path decomposition changes.

## Public contract

`LinkCutForest::rooted_subtree_vertex_count(root, vertex)` returns the number of represented-tree vertices in `vertex`'s subtree when the containing represented tree is rooted at `root`.

- invalid dense vertex IDs throw `std::out_of_range`;
- disconnected `root` / `vertex` pairs throw `std::invalid_argument`;
- the operation may restructure auxiliary splay trees, like the other link-cut queries, but it does not change represented topology or node values;
- `root == vertex` returns the represented component size.

## Represented-cardinality invariant

Each auxiliary node keeps two cardinality summaries in addition to the sealed `auxiliary_size`:

- `virtual_size`: total represented cardinality of represented children that are currently virtual rather than auxiliary children;
- `represented_size`: `1 + virtual_size + represented_size(left) + represented_size(right)`.

`auxiliary_size` still counts only nodes in the auxiliary splay subtree and remains the sequence index used by Phase 38 navigation. The two notions deliberately remain separate.

### Preferred-path transfer during `access`

When `access` splays one represented ancestor:

1. its old auxiliary right child stops being preferred and its `represented_size` is added to `virtual_size`;
2. the new preferred child from the previous iteration leaves virtual accounting, so its `represented_size` is subtracted;
3. the right child pointer is replaced and `pull` recomputes both auxiliary and represented cardinality.

Rotations and lazy reversal do not create or remove represented edges, so `pull` alone preserves represented cardinality through those structural changes. Numeric assignment/addition tags are orthogonal to cardinality.

### Link and cut

A successful `link(first, second)` reroots `first`, exposes `second`, installs `first` as a represented child of `second`, and adds the complete represented cardinality of `first` to `second`'s virtual contribution.

A valid `cut(first, second)` exposes the represented edge as the preferred left edge before detaching it. Because the edge is preferred at the removal point, no separate virtual subtraction is needed; `pull(second)` removes the detached left contribution.

## Query derivation

The query performs `make_root(root)`, verifies connectivity, and then `access(vertex)`. At that point the auxiliary left subtree of `vertex` is precisely the exposed ancestor side toward `root`; the right child is empty. Therefore

`subtree_size = represented_size(vertex) - represented_size(left(vertex))`,

which leaves `vertex` plus all virtual represented descendants.

## Complexity boundary

The implementation adds only constant-time cardinality bookkeeping to link-cut `access`, rotations, `link`, and `cut`. It therefore preserves the standard link-cut-tree **sequence-level amortized `O(log V)`** bound per operation and `O(V)` resident storage. As in sealed Phases 34–38, one individual operation may still take `O(V)` before amortization; no worst-case-per-operation `O(log V)` claim is made.

## Verification

The dedicated tests use an independent adjacency-matrix forest rather than another link-cut implementation.

- chains and stars verify root-sensitive subtree direction;
- disconnected, invalid, cut, and relink cases verify topology boundaries;
- pending path assignment and path addition verify that cardinality remains orthogonal to sealed numeric lazy state;
- a 3,000-step fixed-seed mixed trace interleaves link/cut, point/path assignment, path addition, path sums, Phase-38 kth navigation, and rooted-subtree queries;
- every rooted-subtree expectation is computed by BFS rooting followed by direct descendant traversal;
- `valid_auxiliary_invariants()` additionally checks the internal represented-size arithmetic alongside the sealed auxiliary/numeric invariants.

The randomized differential is executable evidence for the implementation. The amortized complexity statement remains the standard link-cut-tree analysis, not a conclusion inferred from CI timing.
