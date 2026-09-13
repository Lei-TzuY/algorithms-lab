# Scope recovery: scapegoat-tree ordered set

## Coverage decision

A fresh live-state audit at `main@a2bdfde67ec9a4e524712dd7c9abbc32a42aaaff`
after the merged exact Prüfer checkpoint found no scapegoat tree implementation in
default-branch code, pull-request history, or branch names. Nearby structures
already cover B-trees, van Emde Boas sets, Fibonacci/radix heaps, succinct/static
indexes, and dynamic-forest structures, but none use the scapegoat method of
allowing ordinary BST updates and restoring logarithmic height through occasional
whole-subtree rebuilding.

This work follows `docs/scope_recovery_after_phase69.md`. The frozen
compiler/backend frontier and the historical ROADMAP are intentionally untouched.

## Production contract

`algorithms::data_structures::ScapegoatTreeSet` is a first-principles unique
`int64_t` ordered set with fixed balance parameter `alpha = 2/3`.

- `insert`, `erase`, and `contains` implement set semantics;
- duplicate insertion and absent erasure return `false`;
- `values_in_order()` exposes the exact sorted witness;
- `valid_structure()` replays BST ordering, subtree-size metadata, total node
  count, and `n <= q` bookkeeping;
- `height()`, `maximum_size_since_rebuild()`, and insertion/root rebuild counters
  are verification diagnostics;
- ownership is deliberately noncopyable and nonmovable so moved-from metadata
  cannot violate the public structural diagnostics.

No parent pointers, rotations, colors, or randomized priorities are used.

## Rebuild invariants and proof obligations

Let `n` be the current size and `q` the maximum size since the most recent global
shrink rebuild.

Insertion follows an ordinary BST path and updates subtree sizes. If the inserted
node's depth exceeds `floor(log_{3/2}(q))`, the classical scapegoat argument implies
that some ancestor has a child containing more than `2/3` of the ancestor's
subtree. The implementation walks upward to the first such ancestor and rebuilds
that whole subtree from its in-order sequence into a perfectly balanced BST.
Rebuilding preserves the represented key set while restoring logarithmic local
height.

Deletion is ordinary BST deletion. It does not perform local rotations. When
`n < (2/3)q`, the entire remaining tree is rebuilt and `q` is reset to `n`.
Between global rebuilds this gives `q <= 3n/2`; together with the insertion depth
rule, the search height remains `O(log n)`.

The standard scapegoat-tree accounting argument charges the linear work of local
and global rebuilds across sufficiently many updates, giving amortized `O(log n)`
insert and erase. Membership is worst-case `O(log n)`. A rebuild of a subtree of
size `k` is `O(k)` time and temporary storage. The diagnostic full traversal
operations `values_in_order()` and `valid_structure()` are `O(n)`.

These asymptotic statements are proof obligations of the scapegoat invariants;
finite tests are implementation evidence rather than a substitute amortized proof.

## Independent verification

Focused repo-style verification passed before upload:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection and fail-fast options: 4/4.

Deterministic evidence covers empty/basic semantics, duplicates, absent erasure,
`INT64_MIN` / `INT64_MAX`, 512 sorted insertions that force local scapegoat
rebuilds, and a deletion trace that crosses the `2q/3` shrink threshold and forces
a whole-root rebuild.

The primary randomized oracle is an independent `std::set<int64_t>`: one
fixed-seed 30,000-operation trace over a bounded key domain mixes insertion,
erasure, and membership. Every operation checks return-value equality, exact size,
BST/subtree-size validity, `n <= q`, and the public height bound; periodic and final
checks compare the complete in-order sequence.

The randomized trace deliberately does not require a global shrink rebuild to
occur; that event depends on the operation history and is covered by the separate
deterministic threshold regression.

## Scope / non-claims

Exactly four paths change:

- `include/algorithms/data_structures/scapegoat_tree_set.hpp`;
- `tests/test_scapegoat_tree_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this recovery proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or unrelated recovery surface is modified. This slice
adds no map/multiset API, order statistics, split/join, persistence, lock-free
behavior, cache-performance claim, or tunable-alpha surface.
