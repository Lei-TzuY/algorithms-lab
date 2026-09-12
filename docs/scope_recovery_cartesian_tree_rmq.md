# Scope recovery — Cartesian tree + ±1 RMQ

## Coverage decision

Fresh live audit at `main@8d09373c5a4b415989b767b839cfc2a99c58223c`
found no Cartesian-tree / Fischer–Heun-style ±1 RMQ implementation, PR, or
occupied branch. The repository already has an immutable sparse-table RMQ, so
this slice is deliberately not another range-minimum API variant. It adds a
different proof model: Cartesian-tree order structure, Euler-tour LCA reduction,
and ±1 microblock tabulation giving linear preprocessing with constant-time
queries. The long-lived minimum-cycle-basis branch remains occupied and is left
untouched.

`docs/scope_recovery_after_phase69.md` remains the prospective authority; the
historical compiler/backend ROADMAP tail is not used as a frontier.

## Production contract

`CartesianTreeRmq` snapshots a signed-64-bit sequence and exposes:

- the min Cartesian-tree root, parent, left-child, and right-child relations;
- non-empty half-open `range_min_index(begin,end)` and `range_min` queries;
- deterministic **leftmost** minimum semantics for equal values;
- immutable source-value replay plus Euler/microblock structural diagnostics.

Empty input is valid. Invalid indices/ranges reject. Construction fails closed if
the `2n-1` Euler-tour length would not be representable in `size_t`.

## Algorithm / proof obligation

The Cartesian tree is built in one monotone-stack pass. The stack pops only
strictly larger values, so inorder traversal is exactly the original index order,
the tree is a min-heap, and equal minima remain ordered by original index. For
any interval `[l,r)`, the leftmost minimum index is therefore the LCA of vertices
`l` and `r-1` in this tree.

An iterative Euler tour records vertices and depths; consecutive depths differ by
exactly one. Let the Euler length be `m` and choose
`b=max(1,floor(log2(m))/2)`. Each microblock is identified by its length and its
up/down bit pattern, and every within-block range minimum is pretabulated once
per encountered type. Whole-block minima are indexed by a sparse table. A query
combines at most two microblock lookups plus one whole-block sparse lookup.

Correctness relies on the Cartesian-tree/RMQ-to-LCA correspondence and on the
±1 Euler-depth property. Tests are implementation evidence rather than proofs of
those classical results.

## Complexity accounting

Cartesian-tree and Euler construction are `O(n)`. There are
`B=O(n/log n)` blocks. Sorting their constant-size descriptors and constructing
the block sparse table cost `O(B log B)=O(n)`. With
`b=floor(log m/2)`, at most `2^(b-1)=O(sqrt n)` full ±1 block types exist and a
micro table costs `O(b^2)`, so total micro-table preprocessing is
`O(sqrt n log^2 n)=o(n)`. Thus this direct implementation has `O(n)`
preprocessing/storage and `O(1)` RMQ queries.

No succinct-space constant, dynamic update, arbitrary associative range query,
Fischer–Heun implementation identity, or constant-time generic LCA API is
claimed beyond this exact Cartesian/±1 construction.

## Verification

Focused final candidate passes GCC C++20 strict warnings-as-errors, Clang C++20
strict warnings-as-errors, and actual GCC ASan+UBSan.

Committed evidence covers empty/singleton validation, duplicate leftmost ties,
full signed-64 values, 4096-element increasing/decreasing chains (demonstrating
iterative construction/traversal), and 500 fixed-seed arrays of length 0..256.
For each non-empty randomized array, 120 ranges are compared with an independent
linear leftmost-minimum scan. The structural diagnostic independently replays
inorder numbering, parent/child consistency, heap/tie order, Euler length,
first occurrences, and the ±1 depth property.

## Scope

The intended recovery PR changes exactly four paths:

- `include/algorithms/data_structures/cartesian_tree_rmq.hpp`;
- `tests/test_cartesian_tree_rmq_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused proof document.

No CMake, README, stale ROADMAP, recovery-authority, workflow, benchmark, frozen
compiler/backend, minimum-cycle-basis, or temporary-file surface is touched.
