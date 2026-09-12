# Scope recovery: ranked-zeta subset convolution

## Coverage decision

A fresh post-Phase-69 live-state and recovery-branch audit after the merged
Greenwald-Khanna checkpoint found no subset-convolution implementation, no
ranked subset-zeta / Möbius transform surface, and no historical merged code
covering this operation. The already-created `scope-recovery-subset-convolution`
branch was still exactly at current `main`, making it the unoccupied recovery
handoff rather than a competing parallel surface.

This slice follows `scope_recovery_after_phase69.md`. The historical compiler /
backend roadmap is frozen and intentionally untouched. The recovery step also
changes proof model again: it leaves streaming quantile summaries and recovers a
classical transform over the subset lattice.

## Contract

`algorithms::combinatorial::subset_convolution_mod(first, second, modulus)`
accepts two equal-length non-empty `uint64_t` tables in bit-mask subset order.
The common table length must be a power of two, say `N = 2^n`, and
`modulus >= 2`. It returns

`h[S] = sum_{T subseteq S} f[T] * g[S \\ T] (mod modulus)`.

Input coefficients are reduced modulo `modulus`. The modulus need not be prime:
the algorithm needs only addition, subtraction, and multiplication in `Z/mZ`.
The production multiplication path reuses the repository's sealed overflow-safe
`number_theory::multiply_mod`, so the contract supports the full `uint64_t`
modulus range rather than relying on native product wraparound.

Malformed shape or modulus inputs are rejected. Before allocating the ranked
workspace, production checks the `(n+1) * N` element-count multiplication for
`size_t` overflow. Allocation failure itself remains the standard container
failure model.

## First-principles ranked-transform invariant

For each original subset mask `S`, production first places `f[S]` and `g[S]`
only in rank `|S|`. Let `F_k` and `G_k` denote those rank layers. A subset zeta
transform then gives

`Fhat_k[S] = sum_{T subseteq S, |T|=k} f[T]`,

and likewise for `Ghat`.

At each mask, production forms the rank convolution

`Hhat_k[S] = sum_{i=0..k} Fhat_i[S] * Ghat_{k-i}[S]`.

Every ordered pair of disjoint subsets `(A,B)` with `A union B = S` contributes
to rank `|S|`; pairs whose union is a strict subset of `S` are present in the
zeta-domain product too. Applying subset Möbius inversion removes exactly those
strict-subset contributions. Therefore the recovered rank-`|S|` coefficient is
precisely the subset convolution definition above.

Both transforms are implemented directly with the subset-lattice butterfly:
zeta adds the value with one selected bit removed; Möbius performs the inverse
subtraction. No library transform or naïve subset enumeration is used by
production.

## Complexity

For `N = 2^n`:

- ranked zeta and Möbius transforms take `O(n^2 N)` modular additions /
  subtractions across all rank layers;
- rankwise point products take `O(n^2 N)` modular ring operations;
- workspace is `O(n N)` `uint64_t` elements.

Thus the standard ring-operation bound is `O(n^2 2^n)` time and
`O(n 2^n)` workspace. The repository's full-width `multiply_mod` is itself a
bitwise overflow-safe routine, so this statement is a ring-operation bound, not
a claim that every full-width modular multiplication is one machine instruction.

## Verification

Focused repository-style verification passed before upload under:

- GCC C++20 with the repository strict warnings-as-errors;
- Clang C++20 with the same warnings-as-errors;
- actual GCC ASan+UBSan with leak detection and halt-on-error settings.

Deterministic regressions cover malformed lengths/moduli, `n=0`, convolution
identity, composite modulus semantics, rank-layer support, commutativity, and a
full-`uint64_t` known-answer vector computed independently with arbitrary-
precision arithmetic.

The primary fixed-seed differential corpus runs 240 instances with `n` from 0
through 8 and independently chosen moduli in `[2, 1,000,003]`. Expected results
come from direct `O(3^n)` enumeration of every `T subseteq S`. That oracle uses
ordinary multiplication only inside a deliberately small modulus domain where
the product is provably representable, so it does not reuse production's ranked
transform or `multiply_mod` recurrence.

Finite tests are implementation evidence. Correctness for all valid inputs rests
on the ranked-zeta / rank-product / Möbius identity above.

## Non-claims and scope

This recovery does not add OR/AND/XOR convolution APIs, generic semiring
abstractions, arbitrary-precision exact integer subset convolution, transform
benchmarks, or compiler/backend work. It also makes no universal practical-speed
claim against the direct `O(3^n)` method at small `n`.

Exactly three repository files change: the header-only production algorithm,
the already-registered exact-convolution test translation unit, and this proof
document. CMake, README, ROADMAP, recovery authority, workflows, benchmarks, and
the frozen backend surface remain untouched.
