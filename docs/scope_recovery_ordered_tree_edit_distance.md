# Scope recovery: exact ordered rooted-tree edit distance

## Coverage decision

After the persistent HAMT slice merged as
`main@5efef67f011a98d421d30faf7f8660aae4a897e9`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains byte-string Levenshtein edit distance and
rooted-tree maximum-weight independent-set DP, but no ordered rooted-tree edit
distance implementation, branch, or prior implementation PR.

This slice is not a string-edit variant. The state space is a pair of ordered
forests, and deleting a node promotes its ordered children into the deleted
node's position. Sibling order and ancestry therefore constrain admissible
node correspondences.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen historical
compiler/backend frontier is not resumed.

## Public model

`OrderedRootedTree` contains:

- nodes labeled by signed 64-bit integers;
- an ordered child vector per node;
- an optional root index.

Representation rules:

- the empty tree is exactly `nodes.empty() && root == nullopt`;
- a non-empty tree must name one in-range root;
- the root has indegree zero;
- every other node has indegree one;
- every child index is in range;
- all nodes are reachable from the root;
- cycles, repeated-parent structure, disconnected nodes, and malformed empty
  representations are rejected with `std::invalid_argument`.

Child-vector order is semantically significant.

## Edit contract

`ordered_tree_edit_distance(source,target)` returns the exact minimum unit
cost under the standard ordered-tree operations:

- relabel one node: cost 1 when labels differ;
- delete one node: cost 1, promoting its ordered children into its position;
- insert one node: cost 1, the inverse of delete.

Matching equal labels costs zero.

The empty-to-tree distance is exactly the target node count; tree-to-empty is
the source node count.

The function returns only the exact scalar distance in this slice. It does not
claim edit-script reconstruction.

## Postorder preparation

Production validates the tree, then derives a 1-based postorder index.

For postorder node `i`, define `l(i)` as the postorder index of its
leftmost leaf descendant.

- a leaf has `l(i)=i`;
- an internal node inherits the leftmost value of its first ordered child.

For every distinct leftmost value, the largest postorder node carrying that
value is a Zhang-Shasha keyroot. Keyroots are processed in increasing
postorder order.

This ordering guarantees that whenever a forest recurrence refers to the
already-computed distance of a proper subtree pair, that subtree pair has been
solved by an earlier keyroot combination.

## Forest dynamic program

For source keyroot `i` and target keyroot `j`, production solves the two
forests spanning:

- source postorder interval `[l(i), i]`;
- target postorder interval `[l(j), j]`.

Empty-prefix cells are initialized by repeated unit deletion/insertion.

For each postorder pair `(p,q)`:

- erase `p`;
- insert `q`;
- if `l(p)=l(i)` and `l(q)=l(j)`, align the two current rooted trees using
  match/relabel from the previous forest cell;
- otherwise combine the already-computed exact subtree distance
  `tree_distance[p][q]` with the forest prefixes ending immediately before
  those subtrees.

The rooted-tree distance for `(p,q)` is recorded exactly when both current
subtrees share the active forest's left boundaries.

The final answer is the rooted-tree distance of the two postorder roots.

## Independent mapping oracle

Committed randomized verification uses a different characterization rather
than reusing postorder/keyroot forest DP.

For tiny valid trees, the oracle enumerates partial one-to-one node mappings.

A mapping is admissible only when every mapped node pair preserves, in both
directions:

- strict ancestry;
- reverse ancestry;
- left-to-right order of incomparable subtrees.

For a valid mapping `M`, the unit-cost edit value is

`(|T1|-|M|) + (|T2|-|M|) + relabel_mismatches(M)`.

The first two terms are unmatched deletions and insertions; mapped unequal
labels contribute relabel cost. The oracle takes the minimum over all
admissible mappings.

This proof surface is independent of production:

- no postorder recurrence;
- no keyroots;
- no leftmost-leaf forest indexing;
- no reuse of production DP cells.

A supplementary model-level differential run compared the production
recurrence with the mapping oracle on 3,000 random tree pairs of up to five
nodes each and found zero mismatch. This is supporting evidence only; repository
CI remains the integration gate.

## Deterministic tests

Committed tests cover:

- empty/empty, empty/tree, and tree/empty;
- identical singleton and one-node relabel;
- deletion of a root with child promotion;
- sibling-order sensitivity;
- malformed empty representation;
- missing root;
- out-of-range child;
- disconnected structure;
- cycle/root-indegree violation;
- shared child / multiple-parent violation;
- a mixed fixed structural example checked against the mapping oracle;
- 220 fixed-seed random pairs with 0..5 nodes each, checked against exhaustive
  mapping enumeration;
- symmetry;
- node-count-gap lower bound;
- delete-all/insert-all upper bound.

## Complexity boundary

Let the source and target contain `n` and `m` nodes.

Tree validation and postorder preparation are linear in tree size.

The classical Zhang-Shasha keyroot forest formulation has worst-case polynomial
time bounded by `O(n^2 m^2)` and stores the global rooted-tree table in
`O(nm)` space, plus one active forest table whose dimensions are bounded by
`O(nm)`.

This slice deliberately states the conservative worst-case bound rather than a
shape-sensitive optimization claim.

The exhaustive mapping oracle is intentionally exponential and is restricted
to tiny tests.

## Non-claims

This slice does not claim:

- unordered-tree edit distance;
- weighted edit costs;
- edit-script reconstruction;
- subtree move operations;
- Unicode/string semantics;
- memory-optimal Zhang-Shasha variants;
- RTED/APTED compatibility;
- optimal asymptotic tree-edit bounds;
- benchmark-backed performance.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/dynamic_programming/ordered_tree_edit_distance.hpp`;
- `tests/test_ordered_tree_edit_distance_cases.hpp`;
- `docs/scope_recovery_ordered_tree_edit_distance.md`.

Pre-PR hardening includes:

- explicit `size_t` postorder-index overflow rejection before `n+1`
  allocation;
- explicit test inclusion of the exception contract header.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `5efef67f011a98d421d30faf7f8660aae4a897e9`.
