# Scope recovery: slope-trick convex optimization

## Coverage decision

A fresh post-Phase-69 live-code, pull-request-history, and branch audit after the
arbitrary-byte crit-bit checkpoint found no slope-trick / convex piecewise-linear
breakpoint-heap capability in the default branch and no occupied slope-trick
surface. Nearby candidates were rejected rather than duplicated: minimum-mean
cycle, palindromic tree, van-Emde-Boas/x-fast/y-fast predecessor structures,
Chu-Liu-Edmonds arborescence, Euler-tour dynamic forests, stable roommates,
SMAWK, fractional cascading, and chordal/MCS recognition are already present.
Soft heap also appeared uncovered, but slope trick is the smaller reviewable
frontier for this round and deliberately changes proof model from the preceding
compressed-radix string structure to discrete convex optimization.

Prospective authority remains `docs/scope_recovery_after_phase69.md`; the frozen
Phase-45--69 compiler/backend surface and historically drifted `ROADMAP.md` are
untouched.

## Production contract

`algorithms::optimization::SlopeTrick` represents a convex piecewise-linear
function on the integer line through two first-principles binary heaps:

`f(x) = m + sum_{l in L} max(l-x, 0) + sum_{r in R} max(x-r, 0)`.

The maintained invariant is `max(L) <= min(R)` whenever both heaps are non-empty.
The public surface provides:

- checked constant addition;
- `add_x_minus_a(a)` for `max(x-a, 0)`;
- `add_a_minus_x(a)` for `max(a-x, 0)`;
- `add_abs(a)` for `|x-a|`;
- `prefix_min()`, replacing `f(x)` by `min_{y<=x} f(y)`;
- `suffix_min()`, replacing `f(x)` by `min_{y>=x} f(y)`;
- exact minimum value and an interval of minimizers, with missing bounds denoting
  minus/plus infinity;
- breakpoint counts and an explicit structural diagnostic.

The implementation reuses the repository's Phase-1 `BinaryHeap`; it does not use
`std::priority_queue` as the implementation under study.

## Rebalancing proof obligation

When adding `max(x-a,0)` and the largest left breakpoint is `l>a`, the identity

`max(l-x,0) + max(x-a,0)`
`= (l-a) + max(a-x,0) + max(x-l,0)`

moves `l` from the left heap to the right heap, inserts `a` on the left, and adds
`l-a` to the exact minimum. If `l<=a`, the new hinge is inserted directly on the
right. `max(a-x,0)` is symmetric. These transformations preserve the represented
function and the cross-heap ordering invariant. `add_abs(a)` combines the two
hinges; its minimum increase is exactly the distance from `a` to the current
minimizer interval.

Clearing the right heap changes the right-side slope to zero after the current
minimum region and therefore implements `min_{y<=x} f(y)`; clearing the left heap
symmetrically implements `min_{y>=x} f(y)`.

Breakpoint differences are carried through the full `uint64_t` non-negative
range, so a distance larger than `INT64_MAX` is still accepted when adding it to
a negative current minimum produces a representable exact `int64_t` result. The
final public minimum is checked before heap mutation. The contract intentionally
does not promise strong exception safety for allocator failure inside heap growth.

## Verification

Deterministic tests cover the zero function, one-sided hinges, repeated absolute
value additions, finite and half-infinite minimizer intervals, prefix/suffix
minimum transforms, breakpoint diagnostics, and `int64_t` boundary failures.
Overflow regressions verify that a rejected `add_abs` or one-sided hinge leaves
minimum/breakpoint state unchanged, while a `UINT64_MAX` breakpoint distance that
cancels against `INT64_MIN` to produce exact `INT64_MAX` remains accepted.

The primary independent oracle is a direct pointwise function table on integer
coordinates `[-24,24]`. Five hundred fixed-seed trials perform one hundred mixed
operations each with all inserted breakpoints in `[-8,8]`: hinge additions,
absolute values, constants, prefix minima, and suffix minima. After every
operation, production minimum value and every grid point's minimizer-membership
are compared with the direct table, while structural heap invariants are replayed.
The oracle never uses breakpoint heaps or the slope-trick rebalancing identities.

Focused candidate bytes pass GCC and Clang under the repository strict-warning
set and pass an actual GCC ASan+UBSan build. Full repository CI on the exact
remote head remains the merge gate.

## Complexity and non-claims

Let `B` be the resident breakpoint count. Hinge insertion and `add_abs` are
`O(log B)`; minimum/minimizer queries and constant addition are `O(1)`;
`prefix_min` / `suffix_min` destroy one heap and therefore take `O(B)` in this
concrete vector-backed implementation. Storage is `O(B)`.

This recovery does not add coordinate translation, sliding-window convolution,
generic floating-point breakpoints, arbitrary function evaluation, persistent
versions, multidimensional convex optimization, benchmark-backed speed claims,
or a claim that every slope-trick variant is implemented.
