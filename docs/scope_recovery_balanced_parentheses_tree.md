# Scope recovery: balanced-parentheses rooted-tree topology index

## Coverage decision

After the dynamic exact point-quadtree checkpoint entered `main`, a fresh live
code / pull-request / branch audit looked for balanced-parentheses tree indexes,
BP navigation, `find_close` / `enclose`, and equivalent succinct-tree topology
surfaces. No implementation, recovery PR, or occupied branch was found.

The repository already has the sealed Phase-17 `PackedRankSelectBitVector`, plus
wavelet-matrix and Elias-Fano structures that reuse it. This slice changes the
semantic layer instead of adding another rank/select wrapper: it indexes one
rooted ordered tree encoded as a Dyck word and exposes structural navigation.
The frozen Phase-45--69 compiler/backend surface and the historical ROADMAP are
untouched; prospective authority remains `docs/scope_recovery_after_phase69.md`
plus the fresh live coverage audit.

## Production contract

`algorithms::data_structures::BalancedParenthesesTreeIndex` accepts bytes with
`1 = open` and `0 = close`.

- empty input represents an empty topology;
- non-empty input must encode exactly one rooted ordered tree;
- odd length, non-binary bytes, prefix underflow, unclosed nodes, and multiple
  top-level roots are rejected;
- node ids are exactly the preorder ranks of opening parentheses;
- `open_position` / `node_at_open_position` map between preorder ids and BP
  positions through the sealed packed rank/select substrate;
- `find_close` and `enclose` expose the classic BP positional navigation;
- `parent`, `first_child`, `next_sibling`, `depth`, `subtree_size`, and
  `is_ancestor` expose rooted-tree navigation by preorder node id;
- `valid_structure()` independently replays nesting, parent links, close
  positions, and the one-root condition;
- `packed_topology_payload_bytes()` reports only the reused packed BP payload.

The baseline intentionally stores one `size_t` parent entry and one `size_t`
matching-close entry per node. It therefore **does not** claim theoretical
`2n + o(n)` / `n + o(n)` succinct-tree storage.

## Invariant / proof boundary

A construction stack contains exactly the currently open root-to-current path.
When an opening bit is read, the stack top is therefore its parent; when a
closing bit is read, the stack top is exactly the node it matches. Rejecting an
empty stack before the end and requiring the root to close only at the final bit
establishes one valid rooted-tree Dyck word rather than a forest.

Opening-parenthesis order is preorder. Thus `select1(node)` recovers a node's
open position and `rank1(open + 1) - 1` recovers its preorder id. Every subtree
occupies one contiguous balanced-parentheses interval `[open, close]`; counting
opening bits in that interval gives exact subtree cardinality. Immediate bits
after a node's open or matching close characterize first-child and next-sibling
positions. Parent/ancestor claims follow from stack nesting and interval
containment.

These are structural proof obligations. Randomized testing is implementation
evidence, not a replacement for the Dyck-word / preorder arguments.

## Complexity and storage boundary

Let `n` be the number of nodes.

- construction: `O(n)` direct parsing plus construction of the sealed packed
  rank/select bitvector;
- `parent`, `close_position`, `find_close`, `next_sibling`, and
  `node_at_open_position`: `O(1)` direct work under the sealed rank model;
- operations that recover an opening position through current `select1`
  (`open_position`, `enclose`, `first_child`, `depth`, `subtree_size`, and
  `is_ancestor`) inherit Phase-17's current `O(log n)` select boundary;
- explicit navigation tables use `2n * sizeof(size_t)` logical bytes in
  addition to the packed topology payload.

No constant-time-select claim, theoretical succinct-tree bound, dynamic update,
level-ancestor/LCA index, Euler-tour aggregate, or benchmark speedup is implied.

## Verification

Focused final candidate passes:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers empty topology, malformed/non-binary/unbalanced
encodings, a known branching tree, invalid node/position queries, and 130-node
chain/star encodings that cross 64/128-bit packed-word boundaries.

The primary randomized oracle builds 400 fixed-seed ordinary rooted ordered
trees with 0..100 nodes using independent `parent` and `children` vectors, emits
their BP encoding only after the tree is fixed, and then checks every node's
open/close mapping, parent, first child, next sibling, depth, subtree size,
`find_close`, `enclose`, and every ancestor relation against the ordinary tree.
The oracle does not use packed rank/select or BP navigation to derive expected
answers.

An early focused mismatch exposed a test-oracle identity mistake: original
random-tree vertex ids were incorrectly compared with BP preorder ids. The
oracle was corrected to carry an explicit original-vertex -> preorder mapping;
production navigation semantics were unchanged. This is recorded because node
identity is part of the public contract.

## Scope

The intended recovery diff is exactly four paths:

- `include/algorithms/data_structures/balanced_parentheses_tree.hpp`;
- `tests/test_balanced_parentheses_tree_cases.hpp`;
- one registry include in `tests/test_main.cpp`;
- `docs/scope_recovery_balanced_parentheses_tree.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or temporary-file churn is required.
