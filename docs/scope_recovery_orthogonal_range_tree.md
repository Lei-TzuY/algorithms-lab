# Scope recovery: static 2D orthogonal range tree

## Coverage decision

A fresh live audit at `main@8c1f9b406975b20da749f2d46be8d9ad06446f7a` found no static orthogonal range-tree / merge-sort-tree index in live code, pull-request history, or branch state. Existing nearby capabilities cover different proof models: the KD-tree answers nearest-neighbor queries, CDQ performs offline 3D dominance counting, and the wavelet matrix indexes a one-dimensional sequence. The separately named fractional-cascading recovery branch is already occupied, so this slice deliberately avoids that surface.

This work follows `docs/scope_recovery_after_phase69.md`; the frozen compiler/backend frontier and historically stale ROADMAP are untouched.

## Production contract

`OrthogonalRangeTree2D` is an immutable index over input `Point2i` copies.

- every input copy is preserved; duplicate coordinates contribute duplicate count
- the full signed-32 coordinate domain is accepted because the data structure only compares coordinates
- `count_closed(lower, upper)` counts copies in the inclusive rectangle `[lower.x, upper.x] x [lower.y, upper.y]`
- inverted x or y bounds are rejected with `std::invalid_argument`
- the result exposes the exact count plus the number of canonical primary nodes and secondary binary searches used
- `valid_structure()` is an intentionally expensive diagnostic that replays leaf/catalog and merge invariants

## Invariant and correctness obligation

Construction sorts point copies lexicographically by `(x,y)`. For any closed x interval, two binary searches therefore identify exactly one contiguous leaf interval containing precisely the copies whose x coordinate is in range.

A power-of-two primary segment tree decomposes that leaf interval into disjoint canonical nodes. Each primary node stores, with multiplicity, the sorted multiset of y coordinates of exactly the leaves below it. Two binary searches in each selected catalog count exactly the copies whose y coordinate lies in the closed y interval. Because the canonical nodes partition the selected x interval, summing those catalog counts gives the exact rectangle count without double counting or omission.

The theorem-level obligations are the canonical segment-tree decomposition and sorted-catalog binary-search argument. Random tests are implementation evidence, not substitutes for those arguments.

## Verification

Focused pre-upload verification passed under:

- GCC C++20 strict warnings-as-errors
- Clang C++20 strict warnings-as-errors
- actual GCC ASan+UBSan with leak detection and halt-on-error

Deterministic evidence covers empty indexes, inverted bounds, duplicate-copy semantics, full signed-coordinate extremes, exact-point rectangles, input-order replay, and structural replay.

The independent randomized oracle is a direct full scan with no tree or catalog recurrence. Seven hundred fixed-seed point multisets with `0..128` copies each are queried by one hundred random rectangles apiece (70,000 total queries). Production count must equal the direct scan exactly; diagnostics additionally check two secondary searches per canonical node and the logarithmic canonical-node bound.

## Complexity and non-claims

Sorting plus bottom-up catalog merges take `O(n log n)` construction time and `O(n log n)` stored catalog entries. A count query performs two x binary searches, visits `O(log n)` canonical nodes, and performs two y binary searches in each, for `O(log^2 n)` time. Returned state is `O(1)` beyond the index. The diagnostic is slower and excluded from operation-complexity claims.

This slice does **not** claim dynamic updates, point reporting, weighted range sums, higher-dimensional range trees, persistence, fractional-cascading acceleration, or duplicate-coordinate deduplication.
