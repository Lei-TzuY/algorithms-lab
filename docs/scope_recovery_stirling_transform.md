# Scope recovery: exact Stirling transforms

## Coverage decision

After exact ordered rooted-tree edit distance merged as
`main@e86726147076225171aadac9460fcf1dc7e084c4`, a fresh live audit found
zero open pull requests and zero open issues.

Several initially plausible frontiers were rejected before this slice:

- rANS is already occupied by an ahead-of-main implementation branch;
- Kitamasa/nth-term recurrence evaluation is already part of the merged
  Berlekamp-Massey capability;
- subset Möbius/zeta transforms already appear in subset convolution,
  bitwise convolution, and finite-poset inversion surfaces.

Fresh default-branch, branch, and PR-history searches found no Stirling-number,
Stirling-transform, Bell-number, falling-factorial, or set-partition transform
surface.

This slice therefore introduces a distinct exact combinatorial transform rather
than another tree/hash/string/graph-recognition variant.

## Public contract

`stirling_transform_tables(max_n, modulus)` returns two square triangular
tables through row `max_n`:

- `second_kind[n][k] = S(n,k) mod modulus`;
- `signed_first_kind[n][k] = s(n,k) mod modulus`.

`stirling_transform(input, modulus)` computes

`b_n = sum_{k=0}^n S(n,k) a_k (mod modulus)`.

`inverse_stirling_transform(input, modulus)` computes

`a_n = sum_{k=0}^n s(n,k) b_k (mod modulus)`.

The modulus may be prime or composite but must be at least two. Public inputs
must contain canonical residues strictly below the modulus.

The empty vector maps to the empty vector.

## Recurrence proof boundary

The second-kind table uses

`S(n,k) = S(n-1,k-1) + k S(n-1,k)`,

with `S(0,0)=1`.

Combinatorially, in a partition of `n` labelled elements into `k` nonempty
blocks, the newest element either:

1. forms a singleton block, leaving a partition of `n-1` elements into
   `k-1` blocks; or
2. joins one of the `k` existing blocks of a partition of `n-1` elements
   into `k` blocks.

The signed first-kind table uses

`s(n,k) = s(n-1,k-1) - (n-1)s(n-1,k)`,

again with `s(0,0)=1`.

Its absolute value counts permutations of `n` labelled elements with exactly
`k` cycles, while the sign is `(-1)^(n-k)`.

## Transform inversion

The polynomial bases satisfy

`x^n = sum_k S(n,k) x_(k)`

and

`x_(n) = sum_k s(n,k) x^k`,

where `x_(k)` is the falling factorial.

Thus the lower-unitriangular integer matrices `S` and `s` are exact inverses
over the integers.

Reducing an integer matrix identity modulo any `m >= 2` preserves the
identity. Therefore no primality or modular division is required for the
forward/inverse transform pair.

This is why committed verification intentionally includes composite moduli.

## Arithmetic safety

All public residues are canonical.

Addition uses a branch form that avoids overflowing `uint64_t`:

- if `a >= m-b`, return `a-(m-b)`;
- otherwise return `a+b`.

Subtraction similarly avoids underflow by choosing between direct subtraction
and `m-(b-a)`.

Products reuse the repository's sealed
`algorithms::number_theory::multiply_mod`, which computes a modular product
without overflowing `uint64_t`.

No duplicate modular multiplication engine is introduced.

The table API rejects `max_n == SIZE_MAX` before forming the `max_n+1`
dimension.

## Independent combinatorial verification

The strongest coefficient oracle does not use either production recurrence.

### Second kind

For `n <= 7`, tests enumerate every restricted-growth string:

- first element is block zero;
- each later block id is at most one greater than the largest earlier id.

Restricted-growth strings are a canonical one-to-one encoding of set
partitions. Counting strings by number of used blocks therefore independently
computes `S(n,k)`.

### Signed first kind

For `n <= 7`, tests enumerate every permutation of `0..n-1`, explicitly
decompose each permutation into cycles, and count permutations by cycle count.

The expected signed coefficient is the cycle count multiplied by
`(-1)^(n-k)`, then reduced modulo the test modulus.

Neither oracle uses Stirling recurrence, matrix inversion, or production
transform code.

## Transform verification

Committed tests also cover:

- known row `S(4,*) = [0,1,7,6,1]`;
- known signed row `s(4,*) = [0,-6,11,-6,1]`;
- the all-ones forward transform producing Bell numbers
  `1,1,2,5,15,52,203`;
- Bell-vector inverse recovering all ones;
- basis vectors replaying individual transform-matrix columns;
- malformed modulus and non-canonical residue rejection;
- exact dimension-overflow rejection;
- 180 fixed-seed random vectors for each of moduli
  `2, 6, 97, 1000, 18446744073709551557`;
- both forward-then-inverse and inverse-then-forward identity checks.

The full-width modulus case exercises overflow-safe modular addition and the
repository modular multiply without relying on small-word arithmetic.

## Complexity boundary

For transform length `N`, table construction stores two `N x N` triangular
matrices in square-vector form.

- table construction: `O(N^2 log m)` with the repository bit-doubling modular
  multiplication;
- one forward or inverse transform after table construction:
  `O(N^2 log m)`;
- resident table storage: `O(N^2)`.

The current public convenience functions rebuild their table per call, so each
standalone transform has the same conservative `O(N^2 log m)` time and
`O(N^2)` storage bound.

## Non-claims

This slice does not claim:

- asymptotically fast Stirling transforms;
- NTT/FPS acceleration;
- arbitrary-precision exact integer coefficients;
- Bell-number computation beyond the chosen modulus;
- Stirling numbers with nonstandard generalized parameters;
- q-Stirling numbers;
- floating-point transforms;
- benchmark-backed speedups.

## Scope

Exactly three new paths are intended:

- `include/algorithms/combinatorial/stirling_transform.hpp`;
- `tests/test_stirling_transform_cases.hpp`;
- `docs/scope_recovery_stirling_transform.md`.

The test harness auto-discovers `tests/test_*_cases.hpp`, so no CMake or
test-main registration is needed.

No README, historical ROADMAP, recovery authority, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `e86726147076225171aadac9460fcf1dc7e084c4`.
