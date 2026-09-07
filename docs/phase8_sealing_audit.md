# Phase 8 sealing audit

Phase 8 is sealed only after all three ordered implementation slices were merged and the exact integrated `main` checkpoint passed the full repository CI matrix.

## Integrated checkpoints

- Fixed-field NTT / modular convolution merged at `main@11e551f08ec26995ae7707fd951d803dbf879182`; merged-main CI run `34072119275` completed successfully across GCC Release, Clang Release, and GCC ASan+UBSan.
- Exact two-prime CRT convolution merged at `main@e760461691b6ef6d6329180fdfde2677b6a96171`; merged-main CI run `34072662555` completed successfully across all three jobs.
- Formal power series inversion merged at `main@4c703b088d01e8ae5b7aa57ec286f49dd792b37e`; merged-main CI run `34072947782` completed successfully across all three jobs.

The seal does not infer correctness from CI timing. The evidence below comes from implementation contracts, replayable deterministic cases, independent randomized oracles, and explicit representability boundaries.

## Architecture and integration

The phase forms one dependency chain rather than three unrelated algorithms:

1. Phase-7 `number_theory::power_mod` supplies field exponentiation for NTT stage roots, inverse transform lengths, and the initial formal-series constant inverse.
2. One private parameterized radix-2 NTT engine supplies the public `998244353` transform/convolution and the second transform field used by exact CRT convolution.
3. Formal power series inversion delegates every polynomial multiplication to the public `998244353` convolution substrate and adds Newton doubling above it.

No duplicate polynomial multiplier or second modular-exponentiation implementation is introduced by the phase.

## Mathematical and representability boundaries

### Fixed-field transform

`998244353 = 119 * 2^23 + 1` with primitive root `3` is a documented mathematical parameter assumption. The code enforces power-of-two transform lengths through `2^23`; round-trip and convolution tests validate the implementation but are not presented as a proof of the primitive-root theorem.

Butterfly operands are reduced below the modulus, so direct `uint64_t` products stay below `10^18`; the implementation does not rely on unchecked overflow or non-standard wider integers.

### Exact integer convolution

The second transform field `1004535809` reduces the common radix-2 transform limit to `2^21`. The two-prime product is `1002772198720536577`, yielding the unique centered reconstruction interval `[-501386099360268288, 501386099360268288]`.

Before transforming, the implementation proves the conservative bound

`min(n,m) * max(abs(left)) * max(abs(right)) <= floor(M/2)`.

The comparison is division-first and handles `INT64_MIN` magnitude without signed-negation overflow. An input that may be safe only because of cancellation is deliberately rejected when the bound cannot prove uniqueness. Therefore a CRT residue is never exposed as an allegedly exact integer without a reconstruction proof.

### Formal power series inverse

A non-zero constant coefficient is the field-invertibility precondition. Newton doubling uses `B' = B(2-AB)` and squares the error `1-AB`, doubling the valid prefix each iteration.

The public `2^22` coefficient cap is derived from the underlying `2^23` NTT order: at a step targeting `k`, both convolution output lengths are below `3k/2`, whose next power-of-two padding is at most `2^23` when `k <= 2^22`.

## Independent verification

- NTT: forward/inverse round trips for every power-of-two size through 4096; 600 fixed-seed random polynomial pairs compared exactly with a test-only `O(nm)` modular oracle.
- Exact CRT convolution: 500 fixed-seed signed polynomial pairs compared with an independent exact `O(nm)` oracle, plus centered-range, `INT64_MIN * 0`, and conservative-rejection cases.
- Formal power series inverse: 400 fixed-seed random series compared with an independent `O(n^2)` coefficient recurrence whose modular multiplication/exponentiation is test-local and does not call production Newton, NTT, or `power_mod` code.

These oracles are structurally independent from the production recurrence/decomposition they verify. No wall-clock performance claim is made; asymptotic claims are tied to the implemented NTT/Newton structures.

## Audit result

No correctness, integration, oracle-independence, exactness/representability, or complexity-claim blocker was found in the integrated Phase-8 surface.

Two maintainability debts are recorded but are not seal blockers:

- the fixed `998244353` field policy is intentionally visible across the public NTT and formal-power-series surfaces rather than hidden behind a generic field abstraction;
- `number_theoretic_transform.cpp` currently contains the parameterized transform engine, modular convolution, and CRT plumbing in one translation unit.

Both contracts are explicit and independently tested. Refactoring either during the seal would widen scope without changing executable capability, so they remain future architecture debt rather than being churned into the sealing PR.

## Promotion

Phase 8 is sealed at the integrated checkpoint above. The next architectural gap is weighted combinatorial optimization: Phase 6 established capacity-only max flow and cardinality matching, but the repository still lacks a cost objective. Phase 9 therefore starts with minimum-cost maximum flow, where correctness must include both flow feasibility/maximality and cost optimality evidence rather than merely another flow traversal variant.
