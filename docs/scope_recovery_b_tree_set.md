# Scope recovery: first-principles B-tree ordered set

## Why this recovery slice

A fresh live-code and pull-request-history audit after the bounded-universe van
Emde Boas checkpoint found no B-tree / multiway balanced-search-tree capability.
This deliberately changes proof model again: the previous slice studies recursive
universe decomposition, while this slice studies occupancy-balanced multiway
search nodes and structural repair by split, redistribution, and merge. The
frozen Phase-45--69 compiler/backend surface remains untouched, and the stale
historical `ROADMAP.md` is not used as prospective authority.

## Production contract

`algorithms::data_structures::BTreeSet` stores unique signed 64-bit keys and is
configured by a CLRS-style minimum degree `t >= 2`.

- `insert`, `erase`, and `contains` implement set semantics.
- `values_in_order` exposes the exact sorted witness.
- `height` reports root-to-leaf edge height (`0` for an empty/leaf-root tree).
- `valid_structure` replays all public structural invariants.
- cumulative diagnostics expose split, left/right redistribution, merge, and
  root-shrink events so deterministic regressions prove that deletion repair
  paths actually execute.

The implementation is first-principles. No library balanced tree implements the
production structure; `std::set` appears only as an independent test oracle.

## Structural invariants and proof obligations

For minimum degree `t`:

1. Every non-root node stores between `t-1` and `2t-1` strictly increasing keys.
   A non-leaf root stores at least one key; an empty tree is represented by an
   empty leaf root.
2. Every internal node with `k` keys has exactly `k+1` children. Child key ranges
   are strictly separated by the parent keys.
3. Every leaf is at the same depth.
4. The stored key count equals the public set size and every key occurs once.

Insertion never descends into a full child: a full `2t-1` child is split around
its median first, preserving occupancy and search ranges.

Deletion never descends into a child with only `t-1` keys. Before descent it
borrows from an adjacent sibling with at least `t` keys, or merges the child with
a sibling and the separating parent key. Removing an internal key uses its
predecessor/successor when that adjacent child can spare a key, otherwise merges
the two minimum-occupancy children. An empty internal root is replaced by its
only child, decreasing tree height by one.

These are the classical B-tree preservation obligations. Tests exercise and
replay the implementation but do not substitute for the proof argument.

## Complexity and non-claims

Height is `O(log_t n)`. Node search uses first-principles binary search, but the
vector-backed baseline can shift up to `O(t)` keys/children during split,
redistribution, merge, insertion, or erasure. Therefore updates are
conservatively documented as `O(t log_t n)` worst-case time and membership as
`O(log n)` key comparisons, with `O(n)` resident key/node storage plus recursive
stack proportional to tree height.

This is an in-memory educational B-tree, not a disk-page or cache-oblivious
implementation. It makes no B+ tree, multimap, bulk-loading, persistence,
concurrent-access, crash-consistency, external-memory I/O, or universal cache
performance claim.

## Verification

Focused pre-upload verification passes under repository-equivalent strict flags:

- GCC C++20 warnings-as-errors: 4/4 tests;
- Clang C++20 warnings-as-errors: 4/4;
- GCC ASan+UBSan: 4/4.

Deterministic coverage includes invalid minimum degree, empty/basic semantics,
`INT64_MIN`/`INT64_MAX`, duplicate insertion, absent erase, multi-level root
splits, and an adversarial deletion sequence that provably triggers both sibling
borrow directions, merges, and root shrink.

The randomized primary oracle is `std::set<int64_t>`: minimum degrees 2, 3, and 6
each execute 20,000 fixed-seed mixed insert/erase/contains operations. Every
operation checks structural validity and size equality; periodic and final checks
compare the complete in-order key sequence. Additional deterministic traces cover
minimum degrees 2, 3, 5, and 8.
