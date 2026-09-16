# Scope recovery: Queyranne symmetric-submodular minimization

## Coverage decision

A fresh live audit at `main@1e8c33cc58e83694fee6de60628044b9d808f872`
found no Queyranne / pendent-pair / symmetric-submodular-minimization production
API in default-branch code, pull-request history, or branches. The repository
already contains graph min-cut variants, matroid connectivity-related machinery,
and a large post-Phase-69 recovery corpus, but none exposes the more general
value-oracle proof model. Directed arborescence was separately audited and
rejected as a candidate because it is already sealed in Phase 42.

This slice therefore adds a distinct black-box set-function capability rather
than another graph-cut implementation. The frozen Phase-45--69 compiler/backend
surface remains untouched. Prospective scope is governed by
`docs/scope_recovery_after_phase69.md` plus this fresh coverage audit.

## Production contract

`minimize_symmetric_submodular_function(ground_size, oracle)` minimizes a
caller-supplied `int64_t`-valued symmetric submodular function over the non-empty
proper subsets of `[0, ground_size)`.

- `ground_size` must be at least two;
- the oracle must be present;
- every oracle subset is sorted and duplicate-free;
- symmetry and submodularity are caller preconditions, not properties the finite
  query schedule pretends to certify;
- the result returns one deterministic minimizing subset, its exact oracle value,
  and replayable phase / oracle-call / key-evaluation diagnostics;
- no canonical minimizer is claimed when several subsets tie.

Oracle values are signed full-width `int64_t`. Legal-order keys have the form
`f(W union X) - f(X)` and can require one more magnitude bit than `int64_t`.
Production therefore compares those differences with an explicit sign plus
`uint64_t` magnitude representation instead of overflowing signed subtraction or
using floating point.

## Pendent-pair contraction invariant

For each active contracted ground set, production constructs a legal ordering.
It starts from the first active supernode and repeatedly chooses an unchosen
supernode `X` minimizing

`f(W union X) - f(X)`,

where `W` is the union of supernodes already chosen in the current phase.
Deterministic scan order breaks equal-key ties.

For a symmetric submodular function, the last two supernodes `(T, U)` form a
pendent pair: among sets separating `U` from `T`, `U` itself attains a minimum.
Therefore a global non-trivial minimizer can be chosen either as `U` at this
phase or as a set that keeps `T` and `U` together. Production records `f(U)`,
contracts the two supernodes, and repeats. The best recorded expanded supernode
is consequently a global non-empty proper minimizer.

The universal correctness claim comes from Queyranne's pendent-pair theorem and
contraction argument, not from randomized testing.

## Verification

Focused repo-style verification passed before upload under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic cases cover missing / undersized oracle contracts, replay
determinism, a symmetric cardinality function, and a two-element valid symmetric
submodular function whose legal-order difference is
`INT64_MIN - INT64_MAX = -(2^64-1)`, proving the implementation does not rely on
signed `int64_t` subtraction.

The primary randomized oracle uses non-negative weighted hypergraph cut
functions. Hypergraph cut is symmetric submodular but is not implemented through
Queyranne's recurrence. On 500 fixed-seed instances with two through eight
ground elements, the test independently enumerates every non-empty proper subset,
computes its cut value directly, and requires production to return exactly that
minimum value. It also replays the returned witness through the oracle and
requires repeated production calls to return identical diagnostics and witness.

## Complexity / non-claims

Let `n` be the ground-set size and `T_f` the cost of one caller oracle evaluation.
The direct pendent-pair contraction schedule performs `O(n^3)` oracle calls.
Because this educational API materializes explicit sorted element vectors for
oracle subsets, its own subset-union/copy work is conservatively `O(n^4)` in the
worst case in addition to `O(n^3 * T_f)` oracle work. Resident auxiliary state is
`O(n)` elements plus transient subset vectors.

This slice does not claim general unconstrained submodular minimization,
asymmetric objectives, constrained submodular minimization, oracle-axiom
certification, a canonical minimizer among ties, or a faster specialized graph
min-cut bound.

## Scope

Exactly four paths change:

- `include/algorithms/combinatorial/symmetric_submodular_minimization.hpp`;
- `tests/test_symmetric_submodular_minimization_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof/coverage document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or temporary-file surface is changed.
