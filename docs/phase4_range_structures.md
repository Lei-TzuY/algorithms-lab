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

## Frontier

Fenwick tree is the first Phase-4 slice. Segment tree is next so the repository can compare a compact prefix-oriented binary decomposition with an explicit hierarchical range structure rather than merely accumulating unrelated data-structure names.
