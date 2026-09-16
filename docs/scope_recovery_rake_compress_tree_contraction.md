# Scope recovery: deterministic rake/compress tree contraction

## Coverage decision

This slice follows `docs/scope_recovery_after_phase69.md` and a fresh live audit at
`main@8c6fba5df347a5c0eaf0ddc192ec0df0ec5985ec` after the discrete kinetic
aggregate reached merged-main green. Default-branch code search found no
`RakeCompress` / Miller-Reif / tree-contraction capability, PR-history searches
found no rake/compress implementation, and the branch namespace had no matching
recovery surface. Existing HLD, centroid decomposition, link-cut trees, and
Euler-tour trees solve different online/path/decomposition problems and are not
modified here.

## Production contract

`rake_compress_tree_contraction(graph, root)` accepts one non-empty strict
undirected tree over dense vertex ids.

- directed input, self-loops, parallel edges, disconnected input, cycles, and an
  invalid root reject explicitly;
- edge weights are intentionally ignored;
- the caller-selected root is never removed;
- every round first **compresses** a deterministic maximal independent set of
  non-root degree-two vertices, replacing each `u-v-w` by `u-w`;
- it then **rakes** every non-root leaf of the contracted tree;
- the result records every compression/rake step, pre-step neighbors, active
  counts, per-vertex removal round/kind, and the final root;
- `valid_rake_compress_tree_contraction` independently replays the returned
  schedule against the input topology and checks maximality, rake completeness,
  metadata, and final-root uniqueness.

The implementation uses ordered adjacency sets so its complete witness is
independent of input edge insertion order and ignored edge weights.

## Shrinkage / proof obligation

Consider an active tree with `n>2` vertices. Let `L` be its leaves, `D` its
degree-two vertices, and `B` its vertices of degree at least three. For a tree,
`B <= L-2`.

At most one degree-two vertex is the protected root. The eligible degree-two
vertices induce disjoint paths. A maximal independent set on those paths has
size at least `(D-1)/3`, because one selected vertex dominates at most itself
and two eligible neighbors. Compressing such vertices preserves every
unselected neighbor's degree. Therefore all original non-root leaves remain
leaves and the rake step removes at least `L-1` more vertices.

Thus one round removes at least

`(D-1)/3 + (L-1) >= (n-2)/3`,

using `n = L + D + B <= 2L + D - 2`. Consequently, if `n' >= 2`,

`n' - 2 <= (2/3)(n - 2)`.

After `O(log n)` rounds only the protected root remains. This shrinkage argument
is the mathematical proof obligation; finite tests are implementation evidence,
not the proof itself.

## Verification

Focused repo-native bytes pass:

- GCC C++20 repository strict warnings-as-errors: 5/5;
- Clang C++20 repository strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with leak detection / fail-fast: 5/5.

Committed evidence covers all validation failures, singleton/star/path behavior,
a 2,048-vertex adversarial path, ignored signed weights, insertion-order
invariance, deterministic replay, and 400 fixed-seed random trees with 1..96
vertices and random roots.

The primary randomized checker is structurally independent of production
selection: it owns a separate adjacency-set copy, verifies every compression is
legal and independent, verifies maximality by domination of every unselected
eligible degree-two vertex, applies the returned contractions itself, derives
the complete rake set by direct degree scans, checks the shrink inequality, and
requires exactly the requested root to survive. It does not call the production
validator to decide expected schedule legality.

## Complexity / non-claims

This direct educational baseline stores active adjacency in ordered sets and
rescans all vertex ids each round. With `O(log n)` rounds and `O(log n)` set
updates, the conservative sequential construction bound is `O(n log^2 n)` time
and `O(n)` resident topology/result metadata, excluding the returned step list
(which is `O(n)`). The validator has the same conservative asymptotic boundary.

The logarithmic **round depth** is the tree-contraction theorem above; this code
does not claim a parallel runtime, PRAM processor bound, dynamic top-tree API,
path/subtree aggregate framework, or replacement for the sealed dynamic-tree
families.

## Scope

Exactly four paths change: one header-only production API, one native test-case
header, one `tests/test_main.cpp` include, and this focused recovery document.
No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or temporary-file churn is included.
