# Phase 14 — persistent segment tree and versioned state

Phase 14 introduces persistence as an executable data-structure contract. `PersistentSegmentTree` stores immutable versions of a signed-64-bit range-sum array. Point assignment creates a new version by path copying; existing versions remain queryable and unchanged, including when updates branch from arbitrary historical versions rather than only the latest state.

## Public contract

The tree is constructed from an element count (all zeros) or an initial vector. Construction creates version `0`. The public surface exposes `size()`, `version_count()`, `node_count()`, `assign(base_version, index, value)`, and `range_sum(version, begin, end)`.

- Versions are stable zero-based IDs and are never mutated after publication.
- `assign` may branch from any valid version and returns the new version ID.
- Public indices are zero-based; ranges are half-open `[begin,end)`.
- Empty trees still have version `0`; only the empty range can be queried and no point assignment is valid.
- Invalid version IDs, indices, and ranges fail closed with `std::out_of_range`.
- Every stored node sum and every returned range sum must be representable as `int64_t`; arithmetic never wraps.

## Persistence and structural sharing invariant

Every version stores only a root index into an append-only node arena. An assignment copies the nodes on exactly one root-to-leaf search path. Each copied internal node reuses the unchanged child index from the base version and points to the newly copied child on the updated side. No node reachable from an older root is modified.

Therefore an update creates `O(log n)` nodes while all untouched subtrees are structurally shared. `node_count()` makes that storage boundary executable: tests verify each update grows the arena by only the height of one interval-decomposition path while old and sibling versions retain their original answers.

The initial version uses `O(n)` nodes. For `u` successful updates the total arena is `O(n + u log n)` rather than copying `O(n)` state per version.

## Transactional failed-update invariant

Version creation is atomic with respect to the public version/node counts. Before copying a path, `assign` reserves enough capacity for both the new root entry and the maximum path-copy node count. Only after those allocations succeed does it append nodes.

If checked arithmetic fails while rebuilding an ancestor sum, the node arena is resized back to its exact prior count and no root/version is published. A failed update therefore leaves both `version_count()` and `node_count()` unchanged. The regression suite deliberately triggers overflow after a copied leaf already exists so this checks rollback after partial path construction rather than only precondition rejection.

## Checked range sums

Each stored node contains an exact representable subtree sum. A range query decomposes `[begin,end)` into stored subtree sums. Those terms are combined with the same sign-balancing discipline used by the sealed mutable segment-tree surface: when the current accumulator is non-negative it consumes a negative term when available, and vice versa. This avoids a false intermediate overflow when positive and negative terms cancel to a representable exact result.

Construction and update still reject any internal subtree sum that itself is not representable. Persistence does not weaken the Phase-4 checked-arithmetic boundary.

## Complexity

For non-empty trees, construction is `O(n)` time/storage. A successful point assignment takes `O(log n)` time and appends `O(log n)` nodes; selecting the base version is `O(1)`. A range sum visits `O(log n)` canonical interval nodes and uses `O(log n)` temporary terms in the current implementation. Version roots require `O(v)` storage for `v` versions.

No claim of hash-consing, deduplication between equal versions, lazy propagation, or persistent range updates is made because those mechanisms are not implemented.

## Verification

Deterministic tests cover empty-tree semantics, invalid versions/indices/ranges, immutable sibling branches from version zero, path-copy node growth, transactional overflow rollback, and cancellation-heavy representable sums.

The randomized differential test starts from 31 zeros and performs 3,000 point assignments whose base version is chosen uniformly from all existing historical versions. The independent oracle stores plain vector snapshots. After every new version, five random version/range queries are compared with bounded linear summation; the bounded corpus uses values in `[-10,000,10,000]`, so the oracle's maximum absolute full-array sum is only 310,000 and requires no non-standard wide integer type. The newly created version is immediately eligible for queries, and periodic full-range checks replay old versions across the branching version DAG.

Focused GCC and Clang builds use the repository strict warning policy. The same seven-test suite also passes under AddressSanitizer + UndefinedBehaviorSanitizer with leak detection enabled. Full repository CI remains the integration gate before merge.

## Frontier

Phase 14 is sealed after the exact implementation and merged-main CI gates. Its architectural boundary is historical immutability plus branchable versions, `O(log n)` path-copy structural sharing, checked sums, and atomic version publication. The sealing audit found no second persistence slice that would add a distinct correctness model; persistent tries/Fenwick variants, lazy persistent range updates, hash-consing, and equal-version deduplication remain out of scope until a future integration need justifies them. Phase 15 promotes online algorithms and competitive analysis rather than farming persistent-container variants.
