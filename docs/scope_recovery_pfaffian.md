# Scope recovery: prime-field Pfaffian elimination

## Coverage decision

Fresh post-Phase-69 live coverage found no Pfaffian production API or Pfaffian PR. The closest sealed capability is bounded binary permanent, whose proof record explicitly lists hafnian/Pfaffian support as a non-claim. This slice changes proof model after directed Matrix-Tree counting: skew-symmetric paired elimination rather than another graph Laplacian determinant/counting variant.

## Production contract

`pfaffian_mod_prime(matrix, modulus)` computes the Pfaffian of one square skew-symmetric matrix over the prime field `F_p`:

- entries are interpreted modulo `p`;
- the matrix must be square and skew-symmetric after normalization;
- diagonal entries must be zero;
- `p` must be prime; characteristic two is supported with the same alternating-matrix convention (zero diagonal, `A^T=-A`, where `-A=A` in `F_2`);
- `Pf(empty)=1` and odd-dimensional skew matrices return the conventional value zero;
- the full `uint64_t` prime domain is supported through the repository's sealed overflow-safe `multiply_mod`, `power_mod`, and deterministic primality test.

The API is algebraic. It does not claim that an arbitrary graph adjacency matrix has a Pfaffian orientation or that this routine by itself counts graph perfect matchings.

## Elimination invariant / proof obligation

At even pivot index `k`, production finds a nonzero entry in row `k`. If no partner exists, expansion along that row makes the Pfaffian zero. Otherwise a simultaneous row/column transposition moves the partner to `k+1`; one such transposition flips the Pfaffian sign.

For pivot `p=A[k,k+1]`, write remaining couplings as `a_i=A[k,i]` and `b_i=A[k+1,i]`. Factoring the pivot reduces the trailing skew block to

`C'_{ij} = C_{ij} + (b_i a_j - a_i b_j) / p`.

Thus `Pf(A)=p*Pf(C')`, up to the accumulated permutation sign. Repeating eliminates two indices per stage. Correctness relies on the classical Pfaffian paired-elimination / skew Schur-complement identity; tests are implementation evidence rather than a proof of that identity.

## Verification

Focused final candidate passed:

- GCC C++20 strict warnings-as-errors: 5/5;
- Clang C++20 strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan: 5/5.

The focused process caught a hand-written known-answer mistake (`2*7 - 3*6 + 4*5 = 16 mod 17`, not 3) without changing production semantics or weakening warnings.

Committed evidence covers field/shape/skew validation, empty and odd-dimensional conventions, a forced pivot row/column swap, modular input normalization, and the full-width prime `18446744073709551557`.

Primary randomized verification uses 1,200 fixed-seed skew matrices of dimensions 0..8 across small, ordinary 32-bit, and full-width primes. An independent recursive oracle evaluates the defining signed sum over pairings by expansion along the first index; it shares neither elimination nor inverses with production.

Secondary evidence uses 240 additional even-dimensional random matrices and independently enumerates every permutation for the determinant, requiring `Pf(A)^2 = det(A) mod p`. This identity is cross-check evidence, not the primary oracle.

## Complexity / non-claims

With dimension `n`, the direct dense elimination performs `O(n^3)` field updates. Under the repository's repeated-doubling modular multiplication and binary modular exponentiation, the conservative direct bound is `O(n^3 log p + n log^2 p)` time and `O(n^2)` storage.

No arbitrary composite-modulus division, integer/arbitrary-precision Pfaffian, hafnian, Pfaffian-orientation construction, planar perfect-matching counter, sparse/blocked acceleration, or benchmark-performance claim is made.

## Scope

The intended recovery slice changes exactly four paths: header-only production, one dedicated test header, one `test_main.cpp` include, and this proof/boundary document. It does not modify CMake, README, historical ROADMAP, recovery authority, workflows, benchmarks, frozen compiler/backend surfaces, or unrelated recovery work.
