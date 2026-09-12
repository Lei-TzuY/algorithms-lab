# Scope recovery: exact prime-field Strassen matrix multiplication

## Coverage decision

Fresh live-state, pull-request-history, default-branch code, and branch audits at
`main@4f7ca1cb31a09dcdbc0f9ca09232659f320fca93` found no Strassen / fast matrix-
multiplication implementation or occupied Strassen recovery surface. The latest
radix-2 complex FFT checkpoint is merged-main green on GCC release, Clang
release, and GCC ASan+UBSan. The long-lived minimum-cycle-basis branch remains an
occupied surface and is intentionally untouched.

This slice also closes an explicit historical non-claim: the sealed Freivalds
matrix-product verifier and exact Bareiss determinant both state that they do not
provide fast matrix multiplication.

## Production contract

`strassen_matrix_multiply_mod(A, B, p, leaf_size)` multiplies non-empty
rectangular matrices exactly over the prime field `F_p`.

- `p` must be prime and every input coefficient must already be a canonical
  residue `< p`;
- ragged matrices, empty matrices, incompatible shapes, composite moduli, and a
  zero leaf threshold are rejected;
- arbitrary rectangular shapes are zero-padded to the least power-of-two square
  covering rows, shared dimension, and columns, then cropped after multiplication;
- public diagnostics expose padded dimension, recursive call count, Strassen
  node count, and exact scalar-product count;
- leaf kernels use ordinary cubic matrix multiplication, but every internal
  Strassen node performs exactly seven recursive products;
- full-width field multiplication reuses the sealed overflow-safe
  `number_theory::multiply_mod` implementation.

The API deliberately uses a prime field rather than signed integer arithmetic.
That keeps every Strassen addition/subtraction/multiplication exact and avoids
silently assuming that large intermediate Strassen sums fit merely because the
final integer product would fit.

## Algebraic invariant

For block matrices

`A = [[A11,A12],[A21,A22]]`, `B = [[B11,B12],[B21,B22]]`,

production forms the classical seven products

- `M1=(A11+A22)(B11+B22)`
- `M2=(A21+A22)B11`
- `M3=A11(B12-B22)`
- `M4=A22(B21-B11)`
- `M5=(A11+A12)B22`
- `M6=(A21-A11)(B11+B12)`
- `M7=(A12-A22)(B21+B22)`

and reconstructs the four result blocks through the standard Strassen identities.
These are ring identities, so they remain exact in every prime field. Zero
padding preserves the original rectangular product in the cropped leading block.

Correctness of the seven-product identities is a proof obligation; finite tests
are implementation evidence rather than a theorem substitute.

## Independent verification

Focused exact-candidate verification passed before upload under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection: 4/4.

Evidence includes malformed-shape/modulus/residue/leaf validation, rectangular
padding, and a full-width prime `18446744073709551557` known-answer product.
A forced `8x8`, leaf-size-one regression exposes exactly 57 internal Strassen
nodes, 400 total recursive calls, and 343 scalar products, versus 512 scalar
products for one ordinary cubic `8x8` leaf. These counters demonstrate the
seven-product recurrence; they are not a wall-clock performance claim.

The primary randomized oracle is structurally independent: 420 fixed-seed
rectangular matrix pairs with dimensions 1..8 over `F_101` are multiplied by a
direct cubic triple loop whose numeric range is proven far below `uint64_t`
overflow. Complete matrices must match exactly. Every randomized production
product is additionally passed to the sealed Freivalds verifier as secondary
cross-layer integration evidence; Freivalds is not the primary correctness
oracle.

## Complexity and non-claims

For padded dimension `N` and a fixed positive leaf threshold, the recurrence is
`T(N)=7T(N/2)+O(N^2)`, giving `O(N^log2(7))` field multiplication/addition slots.
Because the repository's full-width modular multiplication uses repeated
addition/doubling, a conservative code-local bit-cost view adds an `O(log p)`
factor to scalar products. The direct copy-heavy recursion uses `O(N^2)` peak
matrix storage with a substantial constant factor.

Zero padding can make this implementation unattractive for highly rectangular
matrices; no crossover-performance, cache-optimal, SIMD, BLAS, Winograd,
Coppersmith-Winograd, arbitrary-ring, signed-integer, floating-point, or benchmark
speedup claim is made. The ordinary leaf threshold is an explicit engineering
parameter, not evidence that Strassen is faster at every tested size.

## Scope

Exactly four paths differ from the live base:

- `include/algorithms/linear_algebra/strassen_matrix.hpp`;
- `tests/test_strassen_matrix_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused proof/coverage document.

No CMake, README, historical ROADMAP, scope-recovery authority, workflow,
benchmark, frozen compiler/backend, occupied minimum-cycle-basis, or temporary
files change.
