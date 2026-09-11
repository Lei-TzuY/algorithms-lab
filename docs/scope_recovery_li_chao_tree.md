# Scope recovery — bounded-exact Li Chao minimum tree

## Scope

This recovered slice adds a first-principles Li Chao tree for an online lower
envelope of affine integer lines. The public domain is an inclusive integer
interval `[minimum_x, maximum_x]`; `add_line(m, b)` assigns a stable insertion
id, and `query(x)` returns the minimum exact value together with one deterministic
minimizing line id. Equal values choose the smallest insertion id.

The baseline deliberately implements whole-domain line insertion and point
minimum queries only. It does not add line deletion, segment-restricted lines,
maximum envelopes, floating-point coordinates, or a monotone convex-hull-trick
API merely to increase breadth.

## Exact-arithmetic boundary

Domain endpoints, slopes, and intercepts are restricted to
`[-1'000'000'000, 1'000'000'000]`. Consequently every permitted evaluation
satisfies

`|m*x + b| <= 1'000'000'001'000'000'000 < INT64_MAX`.

Production therefore evaluates lines directly in `int64_t` without overflow,
non-standard `__int128`, saturation, or floating-point approximation. Queries
outside the configured x-domain and coefficients outside the exact boundary are
rejected explicitly.

## Node invariant and insertion proof obligation

Each allocated node represents one inclusive integer x-interval and stores at
most one resident line. After comparing the incoming and resident lines at that
interval's midpoint, the better line under the `(value, line_id)` ordering stays
resident. The losing line is then tested at the interval endpoints.

For two affine lines, their value difference is affine and changes sign at most
once. Therefore a line that does not win at the midpoint can regain strict or
tie-broken advantage on at most one side of the interval. If it wins at the left
endpoint, only the left child can still need it; otherwise, if it wins at the
right endpoint, only the right child can still need it. If it wins at neither
endpoint, it cannot be the preferred line at any integer point in the interval
and is safely discarded. Identical lines are ordered consistently by insertion
id over the whole domain.

Dynamic child allocation means an insertion follows only the routed path and, if
needed, allocates only the first missing node before terminating. Thus each
insertion adds at most one node; there is no dense tree over every x-coordinate.

## Query correctness obligation

For a query coordinate `x`, production follows the unique root-to-leaf interval
path containing `x` and evaluates every resident line on that path. Whenever an
inserted line could remain optimal at `x`, insertion routed that line through
all interval decisions containing `x` until it either became resident or became
provably dominated throughout a containing interval. Hence the minimum resident
candidate on the query path equals the lower envelope at `x`. Equal values use
the smallest line id both during insertion comparisons and final query reduction.

An envelope with no inserted lines returns `std::nullopt` rather than inventing
an infinity sentinel.

## Complexity

Let `U = maximum_x - minimum_x + 1` and let `L` be the number of inserted lines.
Under the bounded public domain:

- `add_line`: `O(log U)` comparisons and recursion depth;
- `query`: `O(log U)` resident-line evaluations;
- tree storage: `O(L)` allocated nodes: an insertion follows only one branch and
  can allocate at most the first missing node on that branch; actual allocation
  is exposed as a diagnostic count;
- each stored line and node uses constant auxiliary state apart from child
  ownership.

These are direct worst-case bounds for this integer-domain implementation, not
an amortized claim and not a coordinate-compressed or floating-domain result.

## Verification

Deterministic tests cover invalid domains and coefficients, empty envelopes,
out-of-domain queries, crossing and parallel lines, duplicate lines, insertion-id
ties, singleton domains, and the full public arithmetic boundaries.

A fixed-seed randomized differential suite runs 500 independent envelopes over
`[-50, 50]`. Each performs 160 mixed line insertions and queries, followed by a
query at every domain coordinate. Every returned `(value, line_id)` pair is
compared with an independent oracle that scans every inserted line directly; the
oracle does not use Li Chao nodes, midpoint routing, or envelope state.

Focused GCC and Clang builds pass the repository strict-warning set. A separate
real GCC AddressSanitizer + UndefinedBehaviorSanitizer build executes the same
four focused tests cleanly. Full-repository GitHub Actions remains the integration
gate before merge.
