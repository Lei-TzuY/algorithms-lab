# Scope recovery: bounded exact linear matroid parity

## Coverage decision

A fresh live-state and recovery-history audit at
`main@64378c1d93057ed9b2928a1e6888b8c3001f5497` found no matroid-parity /
matroid-matching implementation in default-branch code, pull-request history, or
branch names. Existing unweighted/weighted matroid intersection and matroid union
explicitly list matroid parity as a non-claim. The latest merged-main CI is green,
and there were no open PRs or issues before this branch was created.

This slice deliberately stays inside the recovered algorithms/data-structures
mission. It does not resume the frozen Phase-45--69 compiler/backend surface and
does not modify the historically stale ROADMAP or the recovery-authority file.

## Production contract

`maximum_cardinality_linear_matroid_parity(pairs, modulus)` solves a bounded exact
linear-matroid-parity instance over a prime field:

- every ground-set pair contains exactly two vectors from one common ambient
  vector space over `F_modulus`;
- a feasible solution selects whole pairs whose combined vectors are linearly
  independent;
- the objective is maximum selected-pair cardinality;
- pair order is the deterministic search order, but no canonical optimum among
  multiple maximum-cardinality witnesses is claimed;
- input coordinates are normalized modulo the prime field;
- ragged vector dimensions or composite/non-prime moduli reject;
- the educational exact baseline is intentionally bounded to at most 20 pairs
  and ambient dimension at most 128.

The result returns selected pair ids together with search-node, prune, and
rank-bound diagnostics. The selected ids themselves are the replayable witness.

## Search invariant and proof boundary

Production keeps an incremental prime-field linear basis for all vectors of the
currently selected pairs. A pair is included only if inserting both of its
vectors raises rank by exactly two, which is equivalent to preserving
independence of the complete selected union.

At every include/exclude search node, production copies the current basis and
inserts *all* vectors belonging to all still-undecided pairs. If that relaxed
family can increase rank by only `R`, then any feasible completion can add at
most `floor(R/2)` more pairs, because each additionally selected whole pair must
contribute two independent vectors. This is a valid branch-and-bound upper bound;
pruning cannot remove a better feasible solution.

The include/exclude tree is otherwise exhaustive, so a leaf of maximum selected
cardinality is an exact optimum. Correctness comes from exhaustive search plus
the linear-rank upper-bound argument, not from the polynomial algorithms for
linear matroid parity.

This implementation deliberately does **not** claim the Lovasz randomized
algebraic algorithm, Gabow-style polynomial algorithms, general black-box
matroid-parity solvability, or a polynomial runtime.

## Verification

Focused pre-upload verification passed on the final candidate bytes:

- GCC C++20 with repository strict warnings-as-errors: 5/5;
- Clang C++20 with the same strict warnings: 5/5;
- actual GCC ASan+UBSan with fail-fast/leak detection: 5/5.

Deterministic coverage includes prime/modulus validation, ragged vectors,
explicit resource bounds, empty input, a hand-checkable optimum requiring the
search to skip an incompatible pair, full-width prime-field arithmetic using
`18446744073709551557`, deterministic replay, and a rank-bound case pruned at
the root.

Primary randomized evidence is structurally independent from production: 700
fixed-seed systems with 0--8 pairs, dimensions 0--10, and fields selected from
`F_2, F_3, F_5, F_7, F_11` enumerate every pair subset. Each candidate subset's
rank is computed by the repository's sealed finite-field Gauss-Jordan solver,
not by the production incremental basis. Production cardinality must equal the
exhaustive optimum and every returned witness is independently replayed through
that solver.

Finite randomized evidence checks implementation behavior; it is not a proof of
general matroid-parity theory.

## Complexity and non-claims

Let `m <= 20` be pair count and `d <= 128` ambient dimension. The search has at
most `O(2^m)` nodes. Each rank bound may insert `O(m)` remaining vectors into a
copied `d`-coordinate basis, giving a conservative `O(2^m * m * d^2)` prime-field
operation bound for this direct baseline; repository modular multiplication and
inversion add their documented full-width modular-arithmetic costs. Recursive
basis copies make the conservative resident search-state bound `O(m^2 d)` plus
input/result storage.

No polynomial-time matroid-parity bound, arbitrary-precision field, non-prime
ring, weighted parity objective, oracle-matroid parity, representation-specific
Pfaffian acceleration, or benchmark-speedup claim is implied.

## Scope

Exactly four paths differ from the exact green base:

- `include/algorithms/combinatorial/linear_matroid_parity.hpp`;
- `tests/test_linear_matroid_parity_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof/coverage document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or unrelated recovery surface changes.
