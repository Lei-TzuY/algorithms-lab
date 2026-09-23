# Scope recovery: vEB-layout static ordered set

## Coverage decision

After the exact integer-weight alias sampler merged as
`main@c6d249fe2d2f370d3b604062885cde50d420aacf`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains several ordered-set families, including a
bounded-universe van Emde Boas set, B-tree, scapegoat tree, order-statistic
treap, Elias-Fano indexing, x-fast trie, and y-fast trie. None of those encode
the keys in a recursive van-Emde-Boas **memory layout**.

A direct search found no existing implementation, branch, or pull request for
cache-oblivious / vEB-layout static search.

This slice therefore targets a different representation invariant rather than a
new mutable ordered-set update algorithm.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`.

## Public contract

`VebLayoutStaticSet64` is an immutable set of signed 64-bit integers.

Construction accepts arbitrary input order and duplicate values, then sorts and
deduplicates them.

Public executable operations:

- `contains(value)`: exact membership;
- `lower_bound(value)`: smallest stored value greater than or equal to the
  query;
- `predecessor(value)`: greatest stored value less than or equal to the query;
- `rank(value)`: number of stored values strictly smaller than the query;
- `select(order)`: zero-based sorted selection;
- `layout_values()`: diagnostic exposure of physical key order;
- `valid_structure()`: expensive structural replay.

Empty sets are valid. `select(size())` and out-of-range selections return
`nullopt`.

## Logical balanced tree

After normalization, construction first builds a balanced binary-search tree
over the sorted unique values.

For sorted interval `[begin,end)`, the logical root is the midpoint

`begin + (end-begin)/2`.

Each logical node stores:

- its key;
- logical left/right children;
- exact subtree cardinality;
- exact logical subtree height.

The midpoint recurrence gives logarithmic logical height.

## Recursive vEB layout

The final physical order is not inorder, preorder, breadth-first, or the
bounded-universe van Emde Boas structure.

For a logical subtree containing `h` levels:

1. let `upper = ceil(h/2)`;
2. let `lower = h-upper`;
3. recursively emit the top `upper` levels;
4. enumerate every existing subtree root exactly `upper` edges below the
   current root, from left to right;
5. recursively emit each such bottom subtree for up to `lower` levels.

This clusters the top recursive tree and then its bottom recursive subtrees.

For a perfect 15-node tree containing sorted keys 1..15, the committed physical
order is locked as:

`8,4,12,2,1,3,6,5,7,10,9,11,14,13,15`.

After layout order is fixed, logical child identifiers are translated to
physical indices. Searches therefore navigate exact explicit child links; they
do not infer topology from array arithmetic.

## Search invariants

The physical layout changes storage order only. The logical search tree remains
a strict BST.

Membership, lower-bound, and predecessor each descend one logical child per
comparison.

For rank, whenever the query is greater than the current key, production adds

`1 + size(left_subtree)`

and continues right.

For select, production compares the requested zero-based order with the exact
left-subtree cardinality and descends left, returns the current key, or subtracts
the skipped left subtree plus current node and descends right.

## Structural replay

`valid_structure()` performs a defensive inorder replay.

It verifies:

- empty/root consistency;
- root and child indices are in range;
- no node is reached twice or participates in a cycle;
- stored subtree cardinality equals
  `1 + left_size + right_size`;
- inorder traversal visits every physical node exactly once;
- inorder keys are strictly increasing.

Child indices are checked before any diagnostic subtree-size dereference, so
malformed indices fail safely rather than becoming diagnostic out-of-bounds
access.

The replay is intentionally excluded from normal operation complexity claims.

## Complexity boundary

Let `n` be the number of distinct stored values.

Normalization costs `O(n log n)` after copying the input.

The balanced logical tree has `O(log n)` height.

This implementation's straightforward recursive layout builder repeatedly
enumerates cluster frontiers. A conservative construction bound is
`O(n log n)` time and `O(n)` resident storage.

Exact search operations:

- membership: `O(log n)` comparisons;
- lower bound: `O(log n)`;
- predecessor: `O(log n)`;
- rank: `O(log n)`;
- select: `O(log n)`.

Physical storage is `O(n)`.

## Independent verification

Committed tests compare all public ordered-set semantics against independently
normalized sorted vectors using standard binary-search operations.

Coverage includes:

- empty input;
- duplicate normalization;
- exact 15-node recursive layout;
- `INT64_MIN` and `INT64_MAX`;
- all cardinalities 0..128;
- deterministic predecessor/lower-bound gaps;
- rank/select inversion;
- 240 fixed-seed randomized inputs of up to 319 elements;
- 80 additional probes per randomized input.

A supplementary model-level construction check replayed the recursive layout
for every cardinality 1..299 and found no omitted or duplicated logical node;
the perfect 15-node sequence matched the committed expected layout. This is
supporting evidence only, not repository compiler/sanitizer evidence.

## Non-claims

This slice does **not** claim:

- the bounded-universe van Emde Boas predecessor algorithm;
- dynamic insertion or erasure;
- cache-oblivious optimality;
- an ideal-cache miss bound;
- benchmark-backed cache or latency improvement;
- branch-prediction improvement;
- Eytzinger layout equivalence;
- succinct storage.

The name describes the recursive vEB physical layout only.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/data_structures/veb_layout_static_set.hpp`;
- `tests/test_veb_layout_static_set_cases.hpp`;
- `docs/scope_recovery_veb_layout_static_set.md`.

Pre-PR hardening fixed:

- structural replay child-index validation before subtree-size dereference;
- explicit `<iterator>` inclusion for the independent predecessor oracle.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `c6d249fe2d2f370d3b604062885cde50d420aacf`.
