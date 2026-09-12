# Scope recovery: optimal binary search tree via Knuth optimization

This recovery slice adds an exact ordered-search-tree dynamic program whose proof model is absent from the live repository: optimal binary search trees with successful and unsuccessful search weights, accelerated from cubic root enumeration to quadratic time by Knuth's monotone-root theorem.

## Contract

`optimal_binary_search_tree_knuth(successful, unsuccessful)` treats key indices `0..n-1` as already sorted. `successful[i]` is the non-negative weight of a successful search for key `i`; `unsuccessful[g]` is the non-negative weight of the unsuccessful gap `g`, and exactly `n+1` gap weights are required.

The cost convention is explicit: a key or gap at depth `d` contributes `weight * (d+1)`. The empty tree therefore has cost `unsuccessful[0]`.

The result contains:

- an exact bounded 128-bit two-limb weighted search cost;
- the root key, parent relation, key depths, and unsuccessful-gap depths;
- the complete interval-root diagnostic table for half-open key intervals `[i,j)`;
- the number of root candidates actually evaluated.

The implementation is deterministic. Tied candidate roots keep the smallest root examined under the monotone Knuth bounds.

## Recurrence and proof obligation

For half-open interval `[i,j)`, let `E[i,j]` be the minimum weighted cost of keys `i..j-1` plus gaps `i..j`, and let `W[i,j]` be the sum of those successful/gap weights. Empty intervals satisfy

`E[i,i] = q[i]`.

For a non-empty interval,

`E[i,j] = W[i,j] + min_r ( E[i,r] + E[r+1,j] )`, for `i <= r < j`.

The ordinary recurrence is cubic if every root is scanned. For this optimal-BST weight function, Knuth's theorem gives the monotone optimum-root condition

`R[i,j-1] <= R[i,j] <= R[i+1,j]`.

Production searches only that closed root range. The exposed interval-root table makes the monotonicity obligation directly replayable in tests. Summing the candidate-range widths over one interval length telescopes, yielding `O(n)` candidates per length and `O(n^2)` total candidate evaluations.

Correctness relies on the classical optimal-BST recurrence and Knuth monotonicity theorem. Random tests are implementation evidence, not a proof of those theorems.

## Exact arithmetic boundary

Weights are `uint64_t`, but production does not accumulate the DP in `uint64_t`. It uses a first-principles two-limb unsigned 128-bit representation for prefix sums and DP costs, avoiding false rejection when a representable input has an intermediate/final cost above `UINT64_MAX`.

The result exposes both 64-bit limbs. `to_uint64()` succeeds only when the high limb is zero. This is a bounded-exact implementation, not arbitrary precision; any required internal value beyond the two-limb domain fails closed with `std::overflow_error`.

The quadratic state-size guard also rejects unrepresentable table dimensions before allocation.

## Verification

Focused exact-candidate verification passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection: 4/4.

Deterministic evidence includes:

- malformed `n+1` unsuccessful-weight contract;
- empty and singleton trees;
- the classical five-key CLRS frequency vector, scaled to integers, with exact cost `275` and root index `1`;
- all-zero tie determinism;
- exact `UINT64_MAX` final cost and a result whose cost requires the high 64-bit limb.

The primary randomized oracle is structurally independent of Knuth narrowing: 700 fixed-seed instances with `0..20` keys use a full `O(n^3)` DP that scans every root. Production must match its exact optimum cost; the returned parent/depth witness is independently replayed into the weighted cost, and every interval root is checked against the Knuth monotonicity inequalities.

A separate 256-key case replays the full monotonicity table and verifies the implementation's diagnostic candidate count remains at most `2n^2`; this is an executable structural check, not a wall-clock benchmark.

## Complexity and non-claims

The direct implementation uses `O(n^2)` time and `O(n^2)` resident DP/root storage, plus `O(n)` reconstruction state. The full root table is deliberately retained as an educational diagnostic.

No arbitrary-precision arithmetic, floating-point probability API, entropy/code-length claim, Hu-Tucker/alphabetic coding claim, dynamic update support, or claim that the explicit quadratic table is space-optimal is implied.
