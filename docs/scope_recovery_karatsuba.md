# Scope recovery: first-principles Karatsuba decimal multiplication

## Coverage decision

A fresh live audit at `main@280c6313d30e7ee9abea994c5b6d7f13c9f97f77` found no Karatsuba implementation in default-branch code, pull-request history, or the branch namespace. The exact base push CI run `34947988852` completed successfully on GCC release, Clang release, and GCC ASan+UBSan, with zero open pull requests and zero open issues before promotion.

The immediately preceding recovery checkpoint is exact minimum fill-in / chordal completion, a bounded graph subset-DP objective. This slice deliberately changes proof model to exact divide-and-conquer arithmetic instead of extending graph elimination, game-tree search, numerical least squares, or separation-pair analysis.

## Production contract

`karatsuba_multiply_decimal(first, second)` multiplies two non-empty unsigned decimal strings exactly and returns:

- one canonical decimal product with no leading zero except the value `"0"`;
- the total recursive-call count;
- the number of genuine Karatsuba split nodes;
- the number of scalar base-`10^9` limb products executed in schoolbook leaves.

Inputs may contain leading zeros. Any non-decimal byte, sign character, whitespace, or empty operand is rejected with `std::invalid_argument`.

The implementation owns no external arbitrary-precision dependency. Decimal text is parsed into little-endian base-`10^9` limbs. One limb product is below `10^18`; adding one normalized output limb and one carry still stays below `uint64_t`, so every schoolbook leaf is exact without unchecked wraparound or non-standard integer extensions.

## Karatsuba invariant

For a balanced large pair, with split radix `B^m`:

`x = x0 + x1 B^m` and `y = y0 + y1 B^m`.

Production recursively computes

- `z0 = x0*y0`;
- `z2 = x1*y1`;
- `zs = (x0+x1)(y0+y1)`;
- `cross = zs-z0-z2 = x0*y1+x1*y0`;

and returns

`z0 + cross B^m + z2 B^(2m)`.

The cross term is represented as a complete non-negative big integer and uses exact normalized add/subtract helpers. It is not accumulated in a signed coefficient array, so the implementation does not rely on a hidden coefficient-width bound.

Operands with at most sixteen limbs on the shorter side, or with more than a 2:1 limb-size imbalance, intentionally use the quadratic schoolbook leaf. This prevents a highly unbalanced input from creating a mostly-empty Karatsuba recursion tree.

## Verification

Focused final candidate passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak/UB fail-fast: 4/4.

Deterministic evidence covers empty/non-digit rejection, canonical zero and leading-zero semantics, the exact product `123456789 * 987654321 = 121932631112635269`, commutativity, a large known-value differential case, and repeated diagnostic determinism on an input that must enter the recursive Karatsuba path.

The primary randomized oracle is structurally independent from production. Across 600 fixed-seed operand pairs with 1..320 decimal digits, it performs direct grade-school multiplication over individual base-10 digits. It shares neither the base-`10^9` limb representation nor the Karatsuba split/merge recurrence. Product strings must match exactly, including randomized leading-zero cases.

## Complexity and non-claims

For balanced `n`-limb operands above the fixed leaf threshold, the recurrence `T(n)=3T(n/2)+O(n)` gives `O(n^log2(3))` limb-level arithmetic work. Highly unbalanced operands deliberately fall back to `O(mn)` schoolbook work. Recursive vector splitting/addition uses allocation-heavy educational code rather than a tuned scratch-buffer implementation; resident/transient allocation constants are not presented as competitive with production big-integer libraries.

This slice does **not** claim signed-decimal arithmetic, division, a general arbitrary-precision integer type, Toom-Cook/FFT/NTT crossover, constant-time behavior, cryptographic suitability, cache-optimized multiplication, or benchmark-backed speedup over schoolbook multiplication. The diagnostics are structural evidence, not performance measurements.

## Scope

Exactly four paths change:

- `include/algorithms/number_theory/karatsuba.hpp`;
- `tests/test_karatsuba_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_karatsuba.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, minimum-fill, or temporary-file churn is included. Connector-generated file commits should be squash-merged so `main` receives one coherent scope-recovery checkpoint.
