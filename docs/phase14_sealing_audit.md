# Phase 14 sealing audit

Phase 14 is sealed after the persistent segment tree implementation reached merged `main` as `dc4f79efad00b6eaecd1c729c8af269bc0de108f` and the exact merged-main CI run `34164917483` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

## Capability boundary

The phase establishes persistence as a distinct executable state model rather than as another range-query API:

- every published version is immutable and remains queryable;
- updates may branch from any historical version, creating a version DAG rather than a single mutable timeline;
- each successful point assignment path-copies only one root-to-leaf path and reuses untouched subtree nodes;
- `node_count()` makes the `O(log n)` per-update sharing boundary executable;
- failed checked-arithmetic updates publish no version and roll the node arena back to its exact previous size;
- all stored and returned sums remain checked `int64_t`, including cancellation-heavy queries.

The 3,000-update fixed-seed randomized differential test chooses base versions from the complete historical set, immediately queries newly created versions, and compares against independent vector snapshots. Deterministic regressions cover sibling branches, historical immutability, node-growth bounds, invalid versions/ranges, overflow rollback after partial path construction, and representable positive/negative cancellation.

## Integration evidence

The implementation reuses the repository's established checked-sum semantics while adding a new temporal/version dimension. It does not weaken the mutable Phase-4 segment-tree arithmetic contract. Exact candidate CI and merged-main CI both passed the repository's GCC, Clang, and sanitizer matrix; no unresolved implementation or integration blocker remains.

## Why the phase stops here

The educational hypothesis is persistence itself: immutable historical states, structural sharing, branchable versions, and atomic publication. Adding persistent Fenwick trees, persistent tries, or other containers merely to increase algorithm count would largely repeat the same path-copy/versioning contract. Lazy persistent range updates, hash-consing, equal-version deduplication, and other persistence mechanisms remain future extensions only if a later architecture requires a genuinely different invariant.

## Promotion

Phase 15 moves to online algorithms and competitive analysis. The first hypothesis is deterministic LRU paging with an explicit request-by-request hit/fault/eviction trace, an independent exact offline optimum on bounded sequences, and an honest theorem-derived competitive boundary. This introduces a new reasoning model: decisions must be made without future requests while quality is compared against an offline optimum over the same sequence.
