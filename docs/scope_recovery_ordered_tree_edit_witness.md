# Scope recovery: optimal ordered-tree edit witness

## Promotion decision

The exact ordered rooted-tree edit-distance slice is already merged on main as
`e86726147076225171aadac9460fcf1dc7e084c4`. A fresh live audit on
`main@89251400ed4c3e09b9508a3a4a424fede5642419` found:

- zero open pull requests;
- zero open issues;
- no ordered-tree edit witness / mapping implementation;
- no occupied witness branch;
- no prior witness implementation PR.

This slice is therefore a vertical promotion of an existing exact scalar
capability into an independently checkable optimal certificate.

It deliberately does **not** add another tree-distance variant. The new behavior
is the ability to recover and validate one minimum-cost ordered node mapping.

## Public contract

`OrderedTreeEditWitness` contains:

- `distance`: the exact minimum unit-cost ordered-tree edit distance;
- `mapping`: pairs `(source_node, target_node)` using original node indices.

The mapping uses the standard ordered-tree edit interpretation:

- an unmapped source node is deleted;
- an unmapped target node is inserted;
- a mapped pair with unequal labels is relabeled;
- a mapped pair with equal labels has zero relabel cost.

The returned mapping is one-to-one and preserves both ancestry and left-to-right
order.

`ordered_tree_edit_witness(source,target)` returns one deterministic optimal
certificate.

`valid_ordered_tree_edit_witness(source,target,witness)` is an expensive exact
validator. It checks structure, one-to-one mapping, ordered-tree mapping
constraints, certificate cost, and optimality against the already-merged exact
distance implementation.

Malformed input trees retain the existing distance contract and are rejected
with the same structural exceptions.

## Mapping theorem

For ordered rooted trees, the unit insert/delete/relabel edit distance is
equivalently the minimum cost over admissible one-to-one node mappings that
preserve:

- strict ancestry in both directions;
- left-to-right order of incomparable subtrees.

For a mapping `M`, the unit cost is

`
(|T_source| - |M|)
+ (|T_target| - |M|)
+ relabel_mismatches(M).
`

The first term deletes every unmatched source node. The second inserts every
unmatched target node. A mapped unequal-label pair contributes one relabel.

This correspondence is the certificate boundary used by the validator and the
independent test oracle.

## Production construction

Production reuses the already-merged tree structural contract by validating each
tree against an empty counterpart through
`ordered_tree_edit_distance(tree, empty)`. This performs structural validation
without duplicating malformed-tree acceptance logic.

It then prepares:

- 1-based postorder labels;
- leftmost-leaf indices;
- Zhang-Shasha keyroots;
- a postorder-index to original-node-index map.

The global rooted-subtree distance table is recomputed using the same
Zhang-Shasha keyroot/forest recurrence as the exact scalar implementation.

### Deterministic backtracking

Witness recovery starts from the two postorder roots.

For each pending subtree pair, production rebuilds that pair's local forest DP
and records one backtracking action per forest cell:

- delete current source root;
- insert current target root;
- directly align current roots;
- consume an already-solved proper subtree pair.

Tie-breaking is deterministic:

1. direct alignment / proper-subtree reuse;
2. deletion;
3. insertion.

A proper-subtree action records that subtree pair for later recovery and jumps
the current forest cursor to the prefixes preceding both subtrees.

Pending subtree pairs are processed iteratively rather than recursively, so
certificate reconstruction does not depend on C++ call-stack depth.

Recovered postorder pairs are translated back to original node indices and
sorted deterministically.

## Progress / termination invariant

A local `use_subtree` backtrack cannot refer to the exact same active root pair:
the root/root cell shares both active left boundaries and therefore uses the
direct-root recurrence instead.

Every pending subtree pair therefore strictly reduces at least one active
subtree interval.

Within one local forest backtrack, a subtree action skips the complete selected
source and target subtree intervals. The outer cursor never re-enters those
intervals.

These invariants prevent reconstruction cycles and duplicate traversal of the
same local forest region.

## Exact validator

The validator first recomputes the merged exact scalar distance and requires
`witness.distance` to match it.

It then checks:

- every mapped node index is in range;
- source endpoints are unique;
- target endpoints are unique;
- the mapping size does not exceed either tree size;
- certificate cost equals the claimed distance.

For structure preservation, each tree receives preorder-entry and subtree-end
intervals.

For every mapped pair of mapped pairs, the validator compares:

- source-first ancestor of source-second vs target-first ancestor of
  target-second;
- the reverse ancestor relation;
- source-first left of source-second vs target-first left of target-second;
- the reverse left-of relation.

A mismatch rejects the certificate.

Because exact scalar optimality is checked separately, a structurally valid but
non-minimal mapping is also rejected.

The validator is diagnostic and intentionally excluded from fast-operation
claims.

## Verification independence

The committed tiny-tree oracle does not use:

- postorder DP;
- leftmost-leaf indices;
- Zhang-Shasha keyroots;
- production backtracking;
- the production validator.

Instead it exhaustively enumerates partial one-to-one mappings and directly
checks the ordered mapping characterization.

Committed evidence includes:

- empty/empty;
- identical singleton;
- one-node relabel;
- root deletion with child promotion;
- deterministic tie behavior;
- deliberately malformed certificates:
  - wrong distance;
  - duplicate source endpoint;
  - duplicate target endpoint;
  - sibling-order crossing with otherwise correct certificate cost;
  - structurally valid but non-optimal empty mapping;
- malformed input tree rejection;
- 260 fixed-seed random tree pairs of 0..5 nodes checked against exhaustive
  mapping enumeration;
- 120 larger fixed-seed random pairs of 6..25 nodes checked by certificate
  validation and equality with the exact scalar distance.

A supplementary model-level differential pass reproduced the production
backtracking strategy over 20,000 additional random tiny-tree pairs. For every
pair it checked:

- scalar DP distance equals exhaustive mapping optimum;
- recovered mapping certificate cost equals that distance;
- recovered mapping preserves ancestry and left-to-right order.

The model pass found zero mismatch. It is supporting evidence only; repository
compiler/sanitizer CI remains the integration gate.

## Complexity boundary

Let the source and target sizes be `n` and `m`.

The global Zhang-Shasha rooted-subtree table retains the conservative
`O(n^2 m^2)` worst-case time bound and `O(nm)` rooted-distance storage.

Witness reconstruction rebuilds local forest tables only for pending subtree
pairs. A conservative bound treats at most `O(nm)` distinct pending root
pairs, each with a table no larger than `O(nm)`; this remains within
`O(n^2 m^2)` worst-case time.

Only one local backtrack table is retained for active processing, alongside the
global rooted-distance table, pending work, and output certificate, so the
implementation uses conservative `O(nm)` auxiliary storage.

The exact validator additionally performs pairwise mapping-relation checks over
`k=|M|` mapped pairs, adding `O(k^2)` diagnostic work on top of recomputing
the exact scalar distance.

These are conservative correctness boundaries, not performance benchmarks.

## Non-claims

This slice does not claim:

- a directly replayable sequence of mutable insert/delete operations;
- edit-script application to an in-memory mutable tree;
- unordered-tree edit witnesses;
- weighted edit costs;
- subtree move operations;
- minimum-memory reconstruction;
- APTED/RTED compatibility;
- benchmark-backed performance.

The mapping is a compact optimal certificate from which edit categories and
cost are determined, but operation scheduling is outside this slice.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/dynamic_programming/ordered_tree_edit_witness.hpp`;
- `tests/test_ordered_tree_edit_witness_cases.hpp`;
- `docs/scope_recovery_ordered_tree_edit_witness.md`.

Pre-PR hardening includes:

- explicit witness exception-header dependency;
- a crossing-certificate regression whose cost is otherwise valid, so rejection
  specifically exercises order preservation;
- explicit test exception-header dependency.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `89251400ed4c3e09b9508a3a4a424fede5642419`.
