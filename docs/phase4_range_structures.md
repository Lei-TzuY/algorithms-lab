# Phase 4 — range-query and structural data structures

Phase 4 begins with structures whose correctness depends on maintaining a decomposition of an indexed domain rather than solving a one-shot optimization problem. The first slice is a Fenwick tree because its compact binary partition makes both the invariant and the later contrast with segment trees explicit.

## Fenwick tree

The public API is zero-based and uses half-open ranges, while the internal representation is one-based. For internal bucket index `i > 0`, define `lowbit(i)` as its least significant set bit.

**Bucket invariant.** `tree[i]` stores the exact sum of logical array positions in the zero-based interval `[i - lowbit(i), i)`. A point update at logical index `p` begins at one-based `p + 1` and repeatedly advances by `lowbit(i)`, touching exactly the buckets whose represented intervals contain `p`.

A prefix `[0,end)` is decomposed by repeatedly subtracting `lowbit(i)` from `i = end`. Those intervals are disjoint and exactly cover the prefix, so their bucket sums equal the requested prefix sum. A range `[begin,end)` is `prefix(end) - prefix(begin)`.

### Arithmetic semantics

Buckets and returned sums use `int64_t`. Every affected bucket must remain representable. `add(index, delta)` therefore performs a complete checked preflight before writing any bucket; if one bucket would overflow, the operation throws and the entire tree remains unchanged.

A prefix can contain positive and negative bucket values. Even when the exact final prefix is representable, blindly accumulating buckets in one fixed order can transiently overflow. The implementation collects the `O(log n)` Fenwick decomposition terms and alternates opposite signs when available. If the exact final sum is representable, this ordering keeps partial sums representable; if no opposite-sign term remains, representability of the final same-sign remainder guarantees the remaining additions are safe.

Range subtraction is independently checked because two representable prefix sums may have an unrepresentable difference.

### Complexity

- point add: `O(log n)` time
- prefix sum: `O(log n)` time and at most `O(log n)` temporary terms (bounded by the bit width of `size_t`)
- range sum: `O(log n)` time
- storage: `O(n)`

### Verification

Deterministic tests cover empty/bounds behavior, mixed positive and negative updates, all prefixes/ranges on a known sequence, transactional positive/negative bucket overflow, a representable prefix whose naïve Fenwick aggregation order would transiently overflow, and an unrepresentable range difference.

Fixed-seed randomized traces compare point updates, prefix sums, and range sums against a naïve array model. This makes the oracle structurally independent from the binary-indexed decomposition.

## Segment tree

The segment tree rounds the logical length up to a power-of-two leaf base and stores a complete bottom-up binary hierarchy. Unused leaves are zero padding. Public ranges remain zero-based and half-open.

**Node invariant.** Every internal node stores the exact sum of its two child intervals. Therefore each node is an explicit reusable summary of one hierarchical interval, unlike Fenwick's overlapping prefix-oriented buckets.

Construction places logical values at leaves and computes parents bottom-up. `assign(index, value)` replaces one leaf and recomputes exactly its ancestor path. Before changing any stored node, the implementation precomputes every new ancestor value with checked arithmetic; if one would be unrepresentable, the assignment throws and the original tree remains unchanged.

A range query walks the two boundary leaves upward. Whenever a boundary is the outer child of its parent, that canonical node belongs wholly to the query and is collected. These selected nodes are disjoint and exactly cover the requested interval.

### Segment-tree arithmetic semantics

Every node sum must be representable in `int64_t`; construction or assignment rejects states violating that invariant even if cancellation outside that node could make the full-array sum representable. This is a deliberate representation contract: a node that cannot store its own interval summary would break future arbitrary subrange queries.

As with Fenwick queries, selected canonical nodes can have mixed signs. Query aggregation alternates opposite-sign terms so representable exact results are not rejected merely because a fixed traversal order would transiently overflow. If the requested mathematical range sum itself is outside `int64_t`, the query reports `std::overflow_error`.

### Segment-tree complexity and verification

Construction is `O(n)` after power-of-two padding; point assignment and range sum are `O(log n)`; storage is `O(n)`. Deterministic tests cover all subranges of a known sequence, zero initialization, bounds, layout-size rejection, build-time node overflow, transactional positive/negative assignment overflow, transient query aggregation, and unrepresentable query results.

Fixed-seed randomized traces maintain three independent views simultaneously: the segment tree, the existing Fenwick tree, and a naïve array. Point assignments are translated into Fenwick deltas, and every sampled/final subrange must agree across all three representations.

## Comparison boundary

Fenwick trees are compact and naturally express invertible prefix aggregates with point deltas. Segment trees spend a larger constant-factor hierarchy to make arbitrary canonical intervals explicit and are the structural foundation for later non-prefix monoids, richer queries, and lazy propagation. This checkpoint intentionally stops at point assignment + range sum; adding lazy range updates is a future architectural extension, not a near-duplicate API added for count.

## Sparse table — immutable range minimum

The sparse-table slice changes the update/query tradeoff rather than adding a third mutable range-sum structure. It snapshots an input array and precomputes minima for every power-of-two interval length.

Level `k` stores one `Entry{value,index}` for each valid interval of length `2^k`. The base level stores individual elements. Each higher entry combines the two adjacent half-length entries from the previous level, choosing the smaller value and, on equal values, the smaller input index.

**Preprocessing invariant.** After level `k` is built, `levels[k][i]` is the minimum value in `[i, i + 2^k)` and its leftmost occurrence in that interval. The recurrence is correct because the interval is exactly the union of its two `2^(k-1)` halves.

For a non-empty query `[begin,end)`, let `k = floor(log2(end-begin))` and `span = 2^k`. The intervals `[begin, begin+span)` and `[end-span, end)` together cover the requested range and may overlap. Taking the better of their stored entries is still exact because minimum is **idempotent**: seeing an overlapping element twice cannot change the minimum or the leftmost tie-break.

The query API therefore returns both the minimum value and its deterministic leftmost argmin index. Empty ranges are intentionally rejected: unlike sum, minimum has no natural identity value in the signed-64-bit domain exposed by this API. The structure is immutable after construction, so later changes to the caller's input vector cannot affect the snapshot.

### Sparse-table complexity and verification

Preprocessing uses `O(n log n)` time and storage. The precomputed floor-log table makes each non-empty RMQ `O(1)` with two table reads and one deterministic comparison.

Deterministic tests cover all subranges of a known array, duplicate minima/tie behavior, empty and invalid ranges, signed extrema, and snapshot immutability. Fixed-seed randomized arrays are exhaustively checked over every non-empty subrange against a structurally independent linear scan that computes both minimum value and leftmost argmin.

## Frontier

Fenwick and segment trees establish mutable range-query decompositions; sparse table adds the contrasting immutable/preprocessed `O(1)` RMQ regime. Tries are next, moving Phase 4 from numeric index ranges to prefix-structured keys before advanced DSU variants close the structural-data-structure phase.
