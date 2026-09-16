# Scope recovery: exact 2D Gauss lattice-basis reduction

## Coverage decision

A fresh live audit at `main@397af1783b6fcdad3da2ce20d0d53c583a42e3ad`
found no lattice-basis reduction / LLL / Lenstra-Lenstra-Lovasz implementation in
default-branch code or pull-request history, and the branch namespace contains no
lattice-reduction work. The attractive BFPRT selection surface is already occupied
by another recovery branch and is deliberately left untouched. The authoritative
prospective policy remains `docs/scope_recovery_after_phase69.md`; the historical
ROADMAP is not modified.

This slice deliberately chooses the exact two-dimensional Gauss/Lagrange reduction
problem rather than a floating-point general-dimensional LLL implementation. That
keeps the new proof model genuinely lattice-theoretic while preserving the
repository's bounded-exact / fail-closed arithmetic discipline.

## Production contract

`gauss_reduce_lattice_basis_2d(first, second)` accepts two linearly independent
integer vectors with each public coordinate in `[-1,000,000, 1,000,000]`.

It returns:
- a deterministic Gauss-reduced ordered basis `(b1,b2)`;
- a 2x2 signed-64 change-of-basis matrix `T` satisfying
  `(b1,b2)^T = T (input1,input2)^T`;
- the exact count of non-zero size-reduction steps.

The final basis is sign-normalized so each vector's first nonzero coordinate is
positive. Equal-norm final vectors are ordered lexicographically. Dependent bases
are rejected. Out-of-domain coordinates are rejected. Transform bookkeeping uses
checked signed-64 arithmetic and fails closed on an unrepresentable intermediate.
No floating-point arithmetic is used in production.

## Reduction / proof obligations

At each iteration production first puts the shorter vector first. With
`N = <b1,b1>` and `D = <b1,b2>`, it chooses the nearest integer
`q = round(D/N)` with exact half ties toward zero and replaces

`b2 <- b2 - q*b1`.

The same elementary row operation is applied to `T`. Swaps, sign changes, and
integer row addition by a multiple are unimodular operations, so `det(T)=+-1`
and the represented lattice is unchanged.

If `q` is nonzero then `|D| > N/2`; nearest-integer size reduction strictly lowers
`||b2||^2`. After the reduction, the new projection satisfies
`2*|<b1,b2>| <= ||b1||^2`. If the reduced second vector becomes shorter, the
vectors swap and the positive integer squared norm of the new first vector has
strictly decreased. This gives a direct termination measure.

The terminal Gauss conditions are

- `||b1||^2 <= ||b2||^2`, and
- `2*|<b1,b2>| <= ||b1||^2`.

For any nonzero lattice vector `v = m*b1 + n*b2`,

`||v||^2 >= (m^2 + n^2 - |m*n|) * ||b1||^2 >= ||b1||^2`.

Therefore `b1` is a shortest nonzero lattice vector. This two-dimensional theorem
is the mathematical proof obligation; finite randomized tests are implementation
evidence, not a substitute for it.

## Arithmetic boundary

For public coordinate bound `B=1,000,000`, each initial squared norm and absolute
dot product is at most `2*B^2 = 2,000,000,000,000`, and the determinant magnitude
is at most the same bound. Gauss size reduction never increases the currently
reduced vector norm. Geometric scalar arithmetic therefore stays comfortably
inside signed 64 bit for the advertised domain. Change-of-basis metadata is
updated with explicit checked add/subtract/multiply operations and rejects rather
than wraps if an intermediate would exceed its representation.

This is intentionally a bounded-exact educational contract, not arbitrary-
precision lattice arithmetic.

## Verification

Focused final candidate passed before upload under:
- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Committed evidence covers validation, a known hand-replayable reduction, exact
public-coordinate boundaries, deterministic repeated execution, sign/input-order
lattice equivalence, transform replay, `|det(T)|=1`, determinant preservation, and
both Gauss inequalities.

Primary randomized evidence uses 600 fixed-seed independent bases with coordinates
in `[-20,20]`. For every production result the test derives, by Cramer's rule, an
independent finite coefficient box guaranteed to contain every lattice vector no
longer than the returned `b1`; it then exhaustively enumerates that box and checks
that no nonzero vector has smaller squared norm. The oracle does not execute the
Gauss recurrence.

Supplementary pre-upload stress ran 50,000 independent full-domain bases with
coordinates in `[-1,000,000,1,000,000]` under UBSan, replaying transform,
unimodularity, determinant preservation, and reduced inequalities. This stress is
additional robustness evidence only.

## Complexity / non-claims

Each loop performs constant-size checked integer arithmetic. A code-local
conservative termination bound follows from strict descent of a positive integer
squared norm bounded initially by `2*B^2`, giving `O(B^2)` reduction iterations
for the public coordinate bound and `O(1)` resident state. The classical sharper
continued-fraction analysis is not imported as a performance claim for this
bounded implementation.

No general-dimensional LLL, floating Gram-Schmidt, arbitrary-precision lattice
basis, SVP/CVP solver beyond the two-dimensional shortest-vector theorem,
cryptographic lattice security, BKZ, or benchmark-speedup claim is implied.

## Scope

Exactly four paths change:
- `include/algorithms/number_theory/gauss_lattice_reduction.hpp`;
- `tests/test_gauss_lattice_reduction_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_gauss_lattice_reduction.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, KLL, BFPRT, or temporary-file churn.
