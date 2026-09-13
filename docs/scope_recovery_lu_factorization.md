# Scope recovery: dense PLU factorization and solve

## Coverage decision

A fresh live audit after merged x-fast-trie recovery found no LU/PLU/LUP factorization in default-branch code, PR history, or the active recovery namespace. Existing numerical slices cover conjugate gradient, Cholesky, Householder QR, and bidiagonalization. PLU adds a different proof model: row permutation plus unit-lower/upper elimination for general nonsingular dense square systems. Occupied minimum-cycle-basis and Hirschberg-LCS recovery surfaces remain untouched.

## Production contract

`lu_factorize(A)` accepts a finite dense square `double` matrix and returns a permutation, unit-lower `L`, and upper `U` satisfying the executed-arithmetic replay contract `P*A ~= L*U`. `permutation[row]` identifies the source row used by row `row` of `P*A`.

Each column uses deterministic partial pivoting: among remaining rows, choose the largest absolute executed pivot magnitude, keeping the smallest row on ties. An exactly-zero executed pivot throws `std::domain_error`. Required non-finite or non-`double`-representable arithmetic throws `std::overflow_error` rather than propagating NaN/infinity.

`lu_solve(factor,b)` replay-validates permutation/triangular structure, computes `P*b`, then performs unit-lower forward substitution and upper backward substitution.

## Proof obligation

After column `k` is finalized, rows `0..k` of the active matrix represent the corresponding rows of `U`, the stored multipliers below those pivots form `L`, and all performed row swaps are represented by `P`. Gaussian elimination applies elementary lower-triangular elimination matrices, so in exact arithmetic the accumulated identity is `P*A=L*U`. Nonzero pivots make both triangular solves well-defined.

Partial pivoting is a numerical policy, not a proof that IEEE-754 execution is a perfect singularity classifier. This slice claims no condition estimate, numerical-rank threshold, backward-stability theorem, sparse/blocked LU, complete/rook pivoting, BLAS/LAPACK performance, or arbitrary precision.

## Verification

Deterministic evidence covers a matrix requiring an initial row swap, exact factor replay, a known solve, empty input, malformed/non-finite input rejection, singularity, fail-closed overflow, malformed factor rejection, structural zeros/unit diagonal, and deterministic repeatability.

The primary randomized oracle is structurally independent of the production elimination recurrence: 700 fixed-seed systems of size 1..10 are constructed from independently generated unit-lower `L0`, nonsingular upper `U0`, and a random permutation. The original matrix is formed as `P0^{-1} L0 U0`. Production must replay its own `P*A` against its returned `L*U`; independently generated solution vectors are multiplied by the original matrix and must be recovered by `lu_solve`. The production factorization is not compared against the construction factors because PLU factors need not match that generating decomposition after partial pivoting.

Focused strict GCC, strict Clang, and actual GCC ASan+UBSan execution passed 4/4 before upload; GitHub Actions on the exact candidate remains the full integration authority.

## Complexity

Dense factorization performs `O(n^3)` arithmetic and returns `O(n^2)` factor state. Solving from an existing factor is `O(n^2)` time with `O(n)` additional vectors beyond the factors/result.
