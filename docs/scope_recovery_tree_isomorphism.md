# Scope recovery: exact tree isomorphism and canonical rooted structure

## Coverage decision

After exact undirected Chinese postman reached merged-main green, a fresh live
code and PR-history audit found no tree-isomorphism, AHU canonicalization, or
tree-canonical-labeling capability. Minimum cycle basis and planarity were also
uncovered, but both would continue the recent graph-cycle/embedding frontier;
this slice changes the proof model to center invariance plus rooted unordered-tree
canonicalization while remaining small enough for one reviewable vertical slice.

The frozen Phase-45--69 compiler/backend surface remains prospectively out of
scope under `scope_recovery_after_phase69.md`. The historically stale ROADMAP
backend heading is intentionally untouched.

## Production contract

`exact_tree_isomorphism(first, second)` accepts strict undirected trees and
returns either no result or a replayable vertex bijection. Edge weights are
ignored because the problem is unweighted structural isomorphism.

Input rules are explicit:

- empty trees are valid and are isomorphic only to another empty tree;
- directed graphs are rejected;
- self-loops and parallel edges are rejected;
- every non-empty input must be connected with exactly `V-1` logical edges;
- cycles and disconnected inputs are rejected rather than normalized into a
  spanning tree.

For an isomorphic pair, the result contains both inverse mapping vectors and the
canonical one- or two-vertex center sets of each input. Repeated calls are
deterministic. No recursion is used in validation, rooting, center peeling,
signature construction, or witness reconstruction.

## Canonicalization and proof obligations

Every tree has either one center or two adjacent centers, and every graph
isomorphism maps the center set onto the center set. Production therefore fixes
the smallest center of the first tree and tries the one or two centers of the
second tree as candidate roots.

For a rooted tree, each vertex receives an interned signature determined solely
by the sorted multiset of its child signatures. Leaves share the empty-child
signature. Signatures are assigned bottom-up through a shared interner for the
two candidate rootings, so equal root signatures are equivalent to equal rooted
unordered-tree structure.

When root signatures agree, production reconstructs an explicit bijection by
grouping children by signature and pairing equal groups deterministically by
vertex id. Recursive equality of child signatures makes each such paired
subtree isomorphic. Trying every possible image of the chosen center therefore
covers every unrooted isomorphism.

The tree-center theorem and AHU rooted-canonicalization theorem are proof
obligations. Tests provide implementation evidence rather than replacing those
theorems.

## Verification

Focused pre-upload verification passed under:

- GCC C++20 with repository strict warnings-as-errors;
- Clang C++20 with the same strict warnings;
- GCC ASan+UBSan with leak detection and halt-on-error behavior.

The five native test groups cover:

- empty/singleton semantics and size mismatch;
- weighted paths with relabeling, bicentered trees, deterministic repeat output,
  and non-isomorphic path/star shapes;
- rejection of directed input, self-loops, parallel edges, cycles, and an
  `E=V-1` disconnected triangle-plus-isolated input;
- 500 fixed-seed random tree pairs with 0--8 vertices, checked against an
  independent brute-force permutation oracle over adjacency matrices;
- a 4096-vertex path pair, demonstrating that the production path does not rely
  on recursive process-stack depth.

Every returned mapping is independently replayed as a bijection and checked
against all adjacency pairs. The brute-force oracle contains no center peeling,
AHU signature, or production witness logic.

## Complexity and non-claims

Validation and center peeling are `O(V+E)`. Root orientation and witness
reconstruction are linear outside ordering work. This direct educational
implementation uses ordered maps whose keys are sorted child-signature vectors;
to avoid importing a tighter AHU bound without proving it for this exact
container strategy, the public claim is conservatively `O(V^2 log V + E)` time
and `O(V+E)` resident algorithm state plus transient signature-key storage.

This slice does not claim general graph isomorphism, labeled/weighted tree
isomorphism, subtree-isomorphism indexing, automorphism-group enumeration,
canonical vertex numbering independent of input ids, or a strict linear-time AHU
implementation.
