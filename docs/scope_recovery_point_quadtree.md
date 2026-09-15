# Scope recovery: dynamic exact point quadtree multiset

## Coverage decision

A fresh live audit after the persistent balanced byte-rope checkpoint found no quadtree / point-quadtree implementation in default-branch code, pull-request history, or the branch namespace. Existing spatial structures solve different static problems: the sealed 2D KD-tree is an immutable nearest-neighbor index, and the sealed orthogonal range tree is an immutable range-count index. This slice changes the state model to a mutable recursive spatial partition with split and erase-driven compaction.

Prospective work follows `docs/scope_recovery_after_phase69.md`; the stale historical compiler/backend roadmap remains untouched.

## Production contract

`algorithms::data_structures::PointQuadtreeMultiset` stores exact `Point2i` multiplicities in the repository geometry domain `[-1e9,1e9]^2`.

- insert one copy, erase one copy, exact point multiplicity, total size, and distinct-coordinate count;
- inclusive closed-rectangle count and deterministic lexicographically sorted `(point,multiplicity)` reporting;
- duplicate coordinates stay in one leaf entry and do not force subdivision;
- each leaf stores at most eight distinct coordinates unless its integer region is unsplittable;
- overflow of a representable total multiplicity rejects before the public insertion mutates the structure;
- invalid points, inverted rectangles, and rectangle endpoints outside the exact domain reject;
- `node_count`, `height`, and `valid_structure` make subdivision and compaction replayable.

## Spatial invariant / proof boundary

Every node owns one closed integer rectangle. A split uses the exact integer midpoints of each non-singleton axis; the two axis decisions form up to four disjoint child rectangles whose union is the parent rectangle. Every accepted point therefore belongs to exactly one child at every internal node.

When a leaf already contains eight distinct coordinates, inserting a ninth distinct coordinate subdivides the leaf and redistributes the existing entries by those deterministic child regions. Cached `total` and `distinct` values are exact sums of descendants. After an erase, any internal subtree with at most eight remaining distinct coordinates is flattened back into one leaf; this is representation compaction only and does not change multiset semantics.

The root side contains 2,000,000,001 integer coordinates per axis. Each descent halves every still-non-singleton axis, so a point route has at most 31 subdivision levels. The partition/compaction argument is the proof obligation; tests are implementation evidence rather than a replacement for it.

## Verification

Focused candidate bytes passed before remote upload under:
- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers exact domain endpoints, duplicate multiplicity, invalid coordinates/ranges, inclusive range semantics, lexicographic reporting, an observable nine-point split, and erase-driven collapse back to the single root leaf at eight distinct points.

Primary randomized evidence runs 20,000 fixed-seed mixed inserts, erase-one operations, and closed-rectangle queries over a duplicate-heavy coordinate domain. An independent `std::map<Point2i,size_t>` multiset plus direct full scan supplies exact point counts, total/distinct sizes, range counts, and range reports. Structural replay runs throughout and again at the end.

The map/full scan is test-only; production performs no ordered-map or full-scan fallback for point updates or spatial queries.

## Complexity / non-claims

Let `D <= 31` be the fixed domain subdivision depth and `B=8` the leaf distinct-point capacity. Point lookup/insertion/erase perform `O(DB)` direct bucket/path work; erase compaction can traverse only a subtree that has fallen to at most `B` distinct coordinates, so it remains within the same fixed-domain bound. Rectangle count/report visit data-dependent quadtree nodes and remain `O(N + K)` worst-case for `N` resident nodes and `K` reported distinct coordinates. Resident structure is `O(DU)` worst-case for `U` distinct coordinates; multiplicities are stored once per coordinate. `node_count`, `height`, and `valid_structure` are diagnostics and may traverse the resident tree rather than sharing point-operation bounds.

No expected spatial-query bound, nearest-neighbor API, adaptive root expansion, floating-point coordinates, higher-dimensional octree, compressed/loose quadtree, concurrency/persistence, cache-optimal layout, or benchmark speedup is claimed.
