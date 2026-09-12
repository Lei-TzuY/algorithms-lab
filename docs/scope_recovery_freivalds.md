# Scope recovery: replayable Freivalds matrix-product verification

## Coverage decision

A fresh post-Phase-69 live-state audit at `main@d7792e6aeade93d73716d94059a2cab70f7364c2` found no Freivalds verifier, randomized matrix-product equality API, matching historical pull request, or occupied branch. The current merged checkpoint is exact offline dynamic bipartiteness with rollback parity constraints. This slice deliberately changes proof model again: one-sided Monte Carlo algebraic verification rather than another temporal graph, preference-matching, subset-transform, or frozen compiler/backend variant.

`docs/scope_recovery_after_phase69.md` remains the prospective authority. Historical compiler/backend ROADMAP headings are not resumed or edited.

## Production contract

`freivalds_randomized_verify_matrix_product(A, B, C, p, seed, trials)` checks the rectangular claim `A * B = C` over the prime field `F_p`.

- matrices are non-empty, rectangular, and dimension-compatible: `A` is `m x k`, `B` is `k x n`, and `C` is `m x n`;
- every matrix entry must already be a canonical residue in `[0,p)`;
- `p` must be prime and `trials` must be positive;
- each trial samples a replayable vector `r in F_p^n`, then compares `A(B r)` with `C r` using the repository's overflow-safe modular multiplication;
- the result exposes requested/executed trial counts, seed, and the first rejecting trial when one exists;
- bounded sampling is defined explicitly from raw `mt19937_64` output using rejection sampling, so replay does not depend on a standard-library distribution mapping.

`accepted == false` is a deterministic witness that the claimed product is wrong. `accepted == true` means only that every requested randomized check passed; finite randomized verification is intentionally not an exact matrix-equality certificate.

## Correctness / probability boundary

If `A * B = C`, exact field arithmetic makes every trial pass, so the verifier has one-sided error: it never rejects a correct product.

If `D = A * B - C` is nonzero, at least one row of `D` defines a nonzero linear form. For a vector sampled independently and uniformly from `F_p^n`, that form vanishes with probability at most `1/p`. Therefore one classical Freivalds trial falsely accepts with probability at most `1/p`, and `t` independent uniform trials give an upper bound of `p^-t`.

That theorem is a mathematical obligation, not something inferred from finite tests. The implementation's deterministic `mt19937_64` stream plus explicit rejection sampler provides reproducible pseudo-random field elements. It is not a cryptographic RNG, an adversarial-randomness guarantee, or a proof that one fixed seed behaves as independent true randomness. In particular, the committed suite contains a fixed malformed product over `F_2` for which one trial with seed `0` accepts, while another seed rejects; this executable counterexample prevents finite acceptance from being mislabeled as exact equality.

## Verification

Focused exact-content verification passed before upload under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

The repo-native coverage includes malformed shapes, composite modulus, zero trials, non-canonical residues, same-seed replay, an explicit one-trial false acceptance, and a full-width prime-field known-answer product computed independently with arbitrary-precision arithmetic.

For the primary differential corpus, 500 fixed-seed rectangular products use the small prime `101`. The test oracle forms the exact product with ordinary `uint64_t` multiplication/addition whose values are provably far below overflow; it does not call the production modular-multiplication helper. Every exact product must be accepted. One independently chosen output entry is then changed by one modulo `101`, and eight replayable Freivalds trials must reject every committed corpus instance.

The randomized corpus is implementation evidence, not a replacement for the one-sided-error theorem.

## Complexity / non-claims

For `A` of shape `m x k`, `B` of shape `k x n`, and `C` of shape `m x n`, one direct trial performs `O(kn + mk + mn)` field multiply/add operations and uses `O(m + k + n)` temporary vector storage. `t` trials multiply the arithmetic work by `t`. The repository's first-principles `multiply_mod` itself costs `O(log p)` modular additions/doublings in the code-local full-width model.

No deterministic-equality guarantee is claimed for acceptance, no composite-ring Freivalds theorem is claimed, and no cryptographic, adversarial-randomness, benchmark-backed speed, asymptotically fast matrix multiplication, or zero-knowledge verification claim is implied.

## Scope

Exactly three files differ from main:

- `include/algorithms/randomized/freivalds.hpp`;
- append-only coverage in already-registered `tests/test_randomized_min_cut.cpp`;
- this proof document.

No CMake, README, ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or occupied recovery-surface file changes.
