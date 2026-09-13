# Scope recovery: bounded exact binary matrix permanent

## Coverage decision

A fresh live `main`, open-PR/issue, branch, code-search, and PR-history audit after
bounded exact minimum dominating set found no permanent or Ryser implementation.
The repository already contains maximum bipartite matching and many exact
combinatorial solvers, but it does not count perfect matchings in a balanced
bipartite graph.

This slice deliberately changes proof model again.  It adds bounded exact
counting through inclusion-exclusion rather than another branch-and-bound graph
subset search, deletion/contraction recurrence, or decision-only matching API.
The frozen Phase-45--69 compiler/backend surface and stale historical ROADMAP are
untouched; `docs/scope_recovery_after_phase69.md` remains the prospective
authority.

## Production contract

`binary_matrix_permanent(row_masks)` accepts an `n x n` 0/1 matrix represented by
`n` `uint64_t` row masks.  Bit `j` of row `i` is `A[i][j]`.

- `n <= 20` is an explicit exact bound; larger inputs throw rather than hiding
  exponential work;
- bits outside the low `n` columns are rejected;
- the empty `0 x 0` permanent is `1`;
- zero rows or zero columns return `0` without enumerating subsets;
- the result exposes the dimension and the number of Ryser subsets actually
  evaluated;
- execution is deterministic.

For a 0/1 matrix the permanent is exactly the number of perfect matchings in the
associated balanced bipartite graph.

## Ryser / Gray-code obligation

Production uses Ryser's inclusion-exclusion identity

`per(A) = sum_{S subseteq [n]} (-1)^(n-|S|) product_i sum_{j in S} A[i][j]`.

Subsets are visited in Gray-code order.  Consecutive subsets differ in exactly
one column, so all row sums are updated by adding or removing that column rather
than recomputed from scratch.  The direct implementation therefore uses
`O(n * 2^n)` arithmetic operations and `O(n)` mutable row-sum state.

The empty subset contributes zero for every non-empty matrix and is skipped.
Gray transitions are replayable and production fail-closes on an impossible
row-sum underflow.

## Exact arithmetic boundary

The final permanent of an `n x n` 0/1 matrix is at most `n!`; under the explicit
bound,

`20! = 2,432,902,008,176,640,000`.

Ryser's signed intermediate inclusion-exclusion sum can exceed the final answer,
so production does not rely on unchecked `uint64_t` intermediate accumulation or
non-standard wide integer extensions.  It evaluates the formula independently
modulo the coprime moduli

- `p1 = 2,013,265,921`;
- `p2 = 1,811,939,329`.

Their product is

`3,647,915,701,995,307,009 > 20!`

and still fits `uint64_t`.  Chinese-remainder reconstruction is therefore unique
for every supported 0/1 permanent.  The implementation additionally rejects an
internally reconstructed value larger than `n!` as an invariant failure.

## Independent verification

The primary small-instance oracle does not use inclusion-exclusion.  For matrices
with at most eight rows it enumerates every column permutation and directly
counts the permutations whose selected entries are all `1`.

Focused evidence covers:

- `0 x 0`, singleton zero/one, identity, triangular, and all-ones matrices;
- a Hall-deficient matrix with no zero row or zero column but permanent `0`;
- a `13 x 13` all-ones matrix whose exact value `13! = 6,227,020,800` exceeds
  each individual CRT modulus, directly exercising reconstruction;
- malformed high bits, exact `n=20` acceptance through a zero-row fast path, and
  `n=21` rejection;
- 500 fixed-seed random 0/1 matrices with dimensions `0..8`, exact equality to
  permutation enumeration, result determinism, and subset-count diagnostics;
- secondary cross-layer evidence that `permanent > 0` exactly agrees with a
  perfect matching of cardinality `n` from the sealed Hopcroft-Karp solver;
- strict GCC and Clang warning gates plus actual GCC ASan+UBSan execution.

The permutation oracle is the primary implementation check.  Hopcroft-Karp only
checks existence and cannot validate the count.

## Non-claims

This is not an arbitrary-integer or weighted permanent API, a determinant-based
identity, a polynomial-time permanent algorithm, a hafnian/Pfaffian solver, an
approximation scheme, or a practical large-instance counter.  The slice is a
bounded exact first-principles inclusion-exclusion baseline with an explicit
`O(n * 2^n)` cost.
