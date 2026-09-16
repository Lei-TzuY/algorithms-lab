# Scope recovery: exact 1D squared-distance transform

## Coverage decision

A fresh post-Phase-69 audit found no squared-distance-transform / lower-envelope-of-parabolas / Felzenszwalb-Huttenlocher surface in default-branch history or PR history. Existing SMAWK, slope-trick, and min-plus matrix capabilities use different state/proof models. This slice adds the exact one-dimensional transform

`g[x] = min_j (f[j] + (x-j)^2)`

rather than extending the immediately preceding isotonic-regression surface.

## Production contract

`squared_distance_transform_1d(costs)` returns the exact transform value and deterministic leftmost minimizing source index for every integer position `x`.

The explicit exact domain is:

- at most 1,000,000 samples;
- every input cost lies in `[-10^15,10^15]`.

Under those bounds, every index square is below `10^12`, every envelope-intersection numerator has magnitude below about `2.001e15`, every denominator is at most `2e6`, and every returned value remains safely inside signed `int64_t`. Production therefore needs neither floating-point intersections nor non-standard wider integers.

## Lower-envelope invariant

Every candidate source `j` defines a parabola

`P_j(x) = f[j] + (x-j)^2`.

All parabolas share the same `x^2` term, so comparing two sources reduces to comparing affine functions with slopes ordered by source index. For old source `v < q`, the new source `q` is strictly better exactly after

`x > (f[q] + q^2 - f[v] - v^2) / (2(q-v))`.

Production computes the first such integer position with exact signed floor division. Strict improvement is deliberate: at equality the smaller source index remains the public tie winner.

The stack stores candidate source indices with strictly increasing first-active positions. If the new source would become active no later than the previous source, that previous source can never be optimal and is popped. Every source is pushed once and popped at most once. A final forward scan advances monotonically through the envelope.

Thus the direct implementation is `O(n)` time and `O(n)` output/auxiliary storage.

## Verification independence

Focused pre-upload verification passes repository-equivalent GCC and Clang C++20 strict warnings-as-errors and an actual GCC ASan+UBSan build.

Deterministic coverage includes empty/singleton input, equal costs, exact leftmost ties, alternating/negative costs, the public cost boundary, and both domain rejections. Every array of length 0..6 over values `[-2,2]` is also exhaustively enumerated and compared against the direct oracle.

The primary randomized oracle is structurally independent of the envelope construction: 5,000 fixed-seed arrays of length 0..160 are solved by direct `O(n^2)` enumeration of every source for every destination. It compares both exact values and the complete leftmost-argmin vector. A 10,000-sample monotone-cost case additionally probes 100 positions against direct enumeration.

The lower-envelope theorem is the proof obligation; the finite corpus is implementation evidence, not a proof of the universal linear-time/correctness claim.

## Scope / non-claims

The prospective slice should remain four-path scope: one header-only production API, one repo-native test-case header, one include in `tests/test_main.cpp`, and this focused proof document. No CMake, README, historical ROADMAP, recovery authority, workflow, benchmark, frozen backend, or unrelated recovery-surface churn is needed.

No multidimensional distance transform, Euclidean image transform, arbitrary real sample coordinates, arbitrary quadratic coefficient, generic convex-hull-trick container, floating-point API, or benchmark speedup is claimed.
