# Scope recovery: Segment Tree Beats

## Why this slice

The post-Phase-69 recovery boundary keeps `algorithms-lab` focused on substantial algorithms and data structures rather than resuming the frozen compiler/backend track. A live coverage audit found no Segment Tree Beats implementation, `range_chmin` surface, `second_maximum` invariant, pull request, or branch. The current scope-recovery graph/exact-algorithm work is therefore left untouched while this independent data-structure slice adds a different amortized proof model.

## Contract

`SegmentTreeBeats` stores a fixed-length sequence of nonnegative signed-64 values and supports half-open:

- `range_chmin(begin, end, cap)`: replace each selected value by `min(value, cap)`.
- `range_sum(begin, end)`: return the exact signed-64 sum.

Construction rejects negative values and an initial subtree sum outside `int64_t`. Caps must be nonnegative. Under this deliberately bounded contract every update only decreases values and sums, so a successful construction cannot create a later stored-sum overflow. Empty ranges are valid; malformed ranges are rejected.

This is intentionally the core beats mechanism, not a kitchen-sink `chmin/chmax/add` implementation.

## Node invariant

For every represented interval a node stores:

- `sum`: the exact sum of the interval,
- `maximum`: its largest value,
- `second_maximum`: the largest value strictly below `maximum`, or `-1` when no such value exists,
- `maximum_count`: the number of entries equal to `maximum`.

Children merge by summing exact sums, choosing the larger maximum class, and deriving the largest value strictly below the merged maximum. Because all logical values are nonnegative, `-1` is an unambiguous sentinel.

## Range-`chmin` shortcut

For a node and cap `c`:

1. If `maximum <= c`, the update is a no-op.
2. If the node is fully covered and `second_maximum < c < maximum`, only the maximum class changes. The new sum is
   `sum - (maximum - c) * maximum_count`, after which `maximum = c`; every non-maximum value remains unchanged.
3. If `c <= second_maximum`, at least two value classes may change, so the update must descend. In particular, equality `c == second_maximum` is **not** a valid shortcut.

A parent whose maximum was capped propagates that cap to children before a partial update. Const range-sum queries do not mutate the tree; they carry the inherited ancestor cap and compute the equivalent virtual child sum when a delayed cap has not yet been pushed.

## Complexity boundary

- construction: `O(n)`,
- range sum: `O(log n)`,
- range `chmin`: standard Segment Tree Beats amortized `O(log^2 n)`,
- resident storage: `O(n)`.

The amortized update bound is the substantive proof obligation: a full-node shortcut is possible precisely while the cap lies strictly between the largest and second-largest classes; otherwise recursion makes structural progress by changing the relevant extremal-class relationship. This is an amortized statement, not a worst-case `O(log n)` per update claim.

## Verification

Deterministic regressions cover full and partial updates, repeated no-op caps, the critical `cap == second_maximum` descent case, empty input/ranges, `INT64_MAX` singleton reduction, malformed ranges, negative-domain rejection, and construction-time sum overflow.

The primary differential oracle is an ordinary vector that applies every `chmin` element-by-element and sums the requested range directly. A fixed-seed randomized suite executes hundreds of mixed update/query operations over hundreds of small arrays and periodically verifies the full sum, every point, and every prefix. Focused GCC strict-warning, Clang strict-warning, and actual GCC ASan+UBSan builds pass.

## Non-claims

This slice does not implement range `chmax`, range addition, arbitrary signed-value beats, persistent/lazy-general segment trees, wide-integer aggregates, or benchmark-backed performance claims. The existing simpler `SegmentTree` remains a separate point-assignment/range-sum teaching abstraction.
