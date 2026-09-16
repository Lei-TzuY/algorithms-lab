# Scope recovery: optimal alphabetic binary tree via Garsia-Wachs

## Coverage decision

A fresh live audit at `main@9a4662a7e51a90369cf5bf43b836c6463347f66b`
found no Garsia-Wachs, Hu-Tucker, or optimal-alphabetic-code implementation in
default-branch code, PR history, commit history, or the branch namespace. The
sealed length-limited-Huffman checkpoint explicitly lists alphabetic coding as a
non-claim, and the sealed optimal-BST checkpoint likewise explicitly excludes
Hu-Tucker/alphabetic coding. Occupied BFPRT-selection and Count-Sketch surfaces
are deliberately untouched. Prospective authority remains
`docs/scope_recovery_after_phase69.md`; the historical compiler/backend ROADMAP
is not resumed.

## Production contract

`optimal_alphabetic_binary_tree(weights)` accepts an ordered sequence of
non-negative `uint64_t` leaf weights and returns:

- one leaf depth per input position;
- a replayable full binary-tree node array whose in-order leaves are exactly
  `0,1,...,n-1`;
- the root node id, or no root for empty input;
- the exact weighted external path length when representable in `uint64_t`;
- the exact number of Phase-1 compatibility merges.

Empty input returns an empty tree. A singleton has depth zero and cost zero even
when its weight is `UINT64_MAX`. Zero weights and repeated weights are valid.
Any required finite subtree-weight sum or final weighted path length outside the
public `uint64_t` domain rejects with `std::overflow_error`. Structural size /
iterator-domain limits reject with `std::length_error` rather than relying on
unsigned wraparound.

The result is an **alphabetic** tree: the caller's leaf order is preserved. This
is distinct from ordinary Huffman coding, where symbols may be reordered solely
by weight.

## Garsia-Wachs invariant / proof boundary

Phase 1 maintains a sequence of weighted trees bracketed by infinite sentinels.
It repeatedly finds the first consecutive triple `(x,y,z)` with `x <= z`,
combines `x` and `y`, and reinserts their sum immediately after the rightmost
earlier weight at least as large as the sum. The temporary tree may have leaves
out of alphabetic order.

Phase 2 records the depth assigned by that temporary tree to each original leaf.
Phase 3 discards the temporary internal topology and reconstructs the ordered
full binary tree from the same leaf-depth sequence by repeatedly combining
adjacent equal-depth completed subtrees.

Correctness relies on the classical Garsia-Wachs theorem: the compatibility
merges produce an optimal external-path-length depth sequence, and an alphabetic
full binary tree exists with those depths. Phase 3 realizes that ordered tree.
That theorem and its exchange/compatibility argument are mathematical proof
obligations; finite tests are implementation evidence rather than a theorem
proof.

The implementation deliberately uses a direct `std::vector` working sequence.
It scans for the first compatible triple and linearly shifts/reinserts the merged
weight. Therefore the honest direct bound is `O(n^2)` time and `O(n)` auxiliary
working/tree state plus the `O(n)` returned witness. It does **not** claim the
classical balanced-sequence `O(n log n)` implementation bound.

## Independent verification

Focused final production/test bytes passed before upload:

- GCC C++20 repository-equivalent strict warnings-as-errors: 4/4;
- Clang C++20 repository-equivalent strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Focused development caught and fixed a real unsigned representability bug: the
initial node-count guard used `(SIZE_MAX + 1) / 2`, so the addition wrapped before
the comparison and incorrectly rejected every non-empty input. The final guard
uses the equivalent overflow-free `SIZE_MAX/2 + 1` boundary and independently
checks the iterator `ptrdiff_t` domain.

Deterministic evidence covers empty/singleton semantics, `UINT64_MAX`, exact and
overflowing two-leaf costs, all-zero weights, repeated equal weights, alternating
zero/nonzero weights, skewed shapes, and repeated deterministic execution.
Every returned tree is replayed to require:

- exactly `2n-1` nodes for non-empty input;
- full-binary internal nodes and leaf-only leaf records;
- in-order leaf ids exactly `0..n-1`;
- returned depths equal replayed root distances;
- recomputed weighted path length equal the public result;
- exactly `n-1` compatibility merges for `n>0`.

Primary randomized verification uses 900 fixed-seed weight vectors with 0..8
leaves and weights in `0..30`. The oracle does **not** execute Garsia-Wachs: it
enumerates every ordered full-binary-tree shape (the Catalan family),
materializes each shape's leaf-depth vector, computes every weighted external
path length, and takes the global minimum. Production cost must equal that exact
optimum for every case.

## Non-claims

This slice does not claim:

- the `O(n log n)` balanced-data-structure implementation of Garsia-Wachs;
- ordinary/unordered Huffman equivalence for arbitrary input orders;
- Hu-Tucker's original construction as the production algorithm;
- arbitrary-precision weights or costs;
- D-ary alphabetic codes, unequal-letter-cost codes, dynamic updates, or
  bitstream framing;
- that randomized/exhaustive testing proves the Garsia-Wachs theorem;
- any benchmark speedup.

## Scope

Exactly four repository paths change:

- `include/algorithms/coding/garsia_wachs.hpp`;
- `tests/test_garsia_wachs_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_garsia_wachs.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, BFPRT, Count-Sketch, or temporary-file churn.
