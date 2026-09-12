# Scope recovery: bounded exact Smith normal form

## Coverage decision

A fresh post-Phase-69 live-state audit at
`main@d5e562cf6b398684cb10a3ef9df1cf7a2479e40e` found no Smith normal form
implementation, no Smith/Hermite-normal-form recovery branch, no historical Smith
implementation commit, and no matching pull request. Existing bounded-exact
Bareiss determinant and prime-field Gaussian elimination explicitly list Smith
normal form outside their contracts: they answer determinant/field-linear-system
questions, not integer-module equivalence under unimodular row and column
operations.

The occupied fractional-cascading, minimum-cycle-basis, general-graph-isomorphism,
van-Emde-Boas, and push-relabel recovery surfaces are left untouched. The frozen
Phase-45--69 compiler/backend sequence remains out of prospective scope under
`scope_recovery_after_phase69.md`.

This slice therefore changes proof model again after radix-heap monotone shortest
paths: it adds exact canonical integer-matrix reduction with replayable left/right
unimodular witnesses and an independent determinantal-divisor oracle.

## Production contract

`smith_normal_form(matrix)` accepts a rectangular signed-64-bit integer matrix.
An empty outer vector denotes the `0 x 0` matrix; non-empty matrices may also have
zero columns. Ragged rows are rejected.

The result contains:

- a rectangular diagonal matrix `D`;
- a square left transform `U`;
- a square right transform `V`;
- the non-zero invariant factors in order; and
- the rank.

The executable witness identity is

`U * A * V = D`.

Every non-zero diagonal entry is positive and each invariant factor divides its
successor. `U` and `V` are products of elementary integer row/column operations,
so they are unimodular.

The implementation is deliberately bounded-exact. Every production add,
subtract, multiply, quotient edge case, and sign normalization is checked in the
signed-64-bit domain. If an intermediate entry of `D`, `U`, or `V` is not
representable, the call throws `std::overflow_error` instead of wrapping. Thus
this API does **not** promise completion for every input whose final invariant
factors happen to fit in `int64_t`; for example, normalizing the one-entry matrix
`[INT64_MIN]` would require the non-representable positive value `2^63` and is
rejected.

## Reduction invariant

At pivot position `k`, the algorithm first moves a minimum-magnitude non-zero
entry from the remaining submatrix to `(k,k)`. It then alternates Euclidean row
and column reductions:

- a row operation replaces `a[i,k]` by its division remainder modulo the pivot;
- a column operation does the symmetric reduction for `a[k,j]`;
- a non-zero remainder of smaller magnitude is swapped into the pivot position.

After the pivot row and column are clear, the pivot is tested against **every**
entry in the unresolved lower-right block. If some entry is not divisible by the
pivot, its row is added to the pivot row. That exposes a non-divisible value next
to the pivot; the next Euclidean column reduction replaces the pivot by a
strictly smaller gcd-like remainder. Reduction therefore continues until the
pivot divides the whole unresolved block.

Once a pivot is finalized, all later operations are confined to rows and columns
strictly after it. The finalized invariant factor consequently divides every
entry from which later factors are formed. This yields the canonical divisibility
chain rather than merely a diagonal integer matrix.

Every matrix row operation is replayed on `U`; every matrix column operation is
replayed on `V`. Swaps, adding/subtracting integer multiples, and sign flips all
have determinant `+/-1`, providing the unimodular witness obligation.

## Verification

Focused final-candidate evidence passes:

- GCC C++20 with repository strict warnings-as-errors;
- Clang C++20 with the same strict warning set; and
- GCC AddressSanitizer + UndefinedBehaviorSanitizer.

Deterministic coverage includes empty/zero-column matrices, signed singleton
normalization, rank-zero/rank-deficient matrices, rectangular matrices, negative
entries, malformed rows, bounded-overflow rejection, and the important already-
diagonal-but-noncanonical case `diag(6,10) -> diag(2,30)`.

For 600 fixed-seed random `1..4` by `1..4` matrices with entries in `[-4,4]`, the
tests independently enumerate every `k x k` minor and compute its determinant by
a recursive Laplace expansion. The gcd of all `k`-minors is the determinantal
divisor `Delta_k`; the oracle invariant factors are
`Delta_k / Delta_(k-1)`. This route does not reuse production Euclidean
row/column reduction.

Each random result additionally replays `U*A*V == D`, checks that `D` is actually
diagonal with the divisibility chain, confirms the reported rank/factors, and
checks `det(U), det(V) in {+1,-1}` using the independent recursive determinant.

## Complexity and non-claims

The result itself stores `O(mn + m^2 + n^2)` signed integers. Each elementary row
or column operation touches one matrix row/column plus its corresponding
transformation row/column. Euclidean remainders strictly decrease the active
non-zero magnitude, but coefficient growth is input-dependent; this bounded
first-principles implementation intentionally makes no strongly-polynomial bit-
complexity or arbitrary-precision claim.

This slice does not implement Hermite normal form, arbitrary-precision Smith
normal form, sparse-matrix acceleration, integer-kernel/module solving wrappers,
fast modular reconstruction, or a universal overflow-free completion guarantee.
Those would require separate proof and representation boundaries rather than thin
extensions of this checkpoint.
