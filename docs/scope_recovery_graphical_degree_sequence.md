# Scope recovery: graphical degree sequences via Erdős-Gallai and Havel-Hakimi

## Coverage decision

A fresh live audit at `main@6d646bc973580822d0184a93c43717a0889afe3c` found no Havel-Hakimi, Erdős-Gallai, graphical-degree-sequence, or simple-graph degree-realization capability in default-branch code or pull-request history. Branch search found no Havel-Hakimi surface; the only `degree` branch-name hits are unrelated finite-field factorization work. The occupied BFPRT-selection and Count-Sketch recovery surfaces are deliberately untouched.

The sealed Prüfer-code slice solves a different problem: it is a bijection between already-valid labeled trees and sequences. This slice decides whether an arbitrary degree vector is realizable by a simple graph and constructs a replayable witness when it is.

`docs/scope_recovery_after_phase69.md` remains the prospective authority; the frozen compiler/backend excursion and stale historical ROADMAP are untouched.

## Production contract

Two first-principles formulations are exposed over dense vertex ids `0..n-1`:

- `erdos_gallai_graphical(degrees)` decides graphicality through the Erdős-Gallai inequalities;
- `havel_hakimi_realization(degrees)` either returns `nullopt` or a deterministic simple-edge witness.

Every public degree must be smaller than `n`; invalid simple-graph degrees throw. The empty vector and singleton `[0]` are graphical. Non-graphical but well-formed vectors return `false` / `nullopt`, not an exception.

Havel-Hakimi ties are deterministic: residual degree descending, then original vertex id ascending. The selected highest-degree vertex connects to the next `d` residual vertices in that order. Returned edges are canonical `(min,max)` pairs sorted lexicographically. `valid_degree_sequence_realization` independently replays simple-edge shape and exact per-vertex degrees.

## Proof obligations

For a non-increasing sequence `d_1 >= ... >= d_n`, the Erdős-Gallai theorem states that it is graphical exactly when the total degree is even and, for every `k`,

`sum_{i<=k} d_i <= k(k-1) + sum_{i>k} min(d_i,k)`.

Production evaluates these inequalities directly after sorting. The right-hand side is intentionally scanned directly instead of importing a faster prefix/binary-search implementation.

The Havel-Hakimi theorem states that a non-increasing sequence with first degree `d` is graphical exactly when deleting that first term and subtracting one from the next `d` terms yields a graphical sequence. Repeating this reduction either reaches all zero residual degrees or exposes an impossible reduction. Recording every reduction edge reconstructs a realizing simple graph.

These classical characterization theorems are mathematical proof obligations. Agreement between the two production formulations is useful cross-evidence but is not treated as an independent proof.

All finite integer accumulations used by the Erdős-Gallai inequalities are checked in `uint64_t`; an unrepresentable in-memory arithmetic instance fails closed with `std::overflow_error` rather than wrapping.

## Independent verification

Focused exact candidate execution passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast leak/UB detection: 4/4.

Deterministic evidence covers empty/singleton semantics, a known graphical sequence, odd-sum and inequality failures, invalid degrees, deterministic tie handling, exact witness replay, and duplicate-edge rejection.

The primary oracle is structurally independent from both characterizations. For every `n=0..6`, tests enumerate every one of the `2^(n choose 2)` labeled simple graphs, compute its degree vector directly, and store the exact realizable set. Tests then enumerate every well-formed degree vector in `[0,n-1]^n`; both Erdős-Gallai and Havel-Hakimi existence must match that graph-enumeration set exactly, and every constructive result is replayed edge by edge.

Additional fixed-seed evidence includes 1,200 arbitrary degree vectors through 40 vertices comparing both production formulations and 500 independently generated simple graphs through 50 vertices whose derived degree vectors must be realized exactly. These corpora are implementation evidence, not proofs of either theorem.

## Complexity and non-claims

The direct Erdős-Gallai baseline sorts once and scans every suffix for every `k`, so it claims `O(n log n + n^2)` time and `O(n)` working storage. Havel-Hakimi re-sorts the residual vector after each reduction, giving a conservative `O(n^2 log n + E)` time bound and `O(n+E)` result/working storage for returned edge count `E`.

No linear-time graphicality test, canonical graph up to isomorphism, connected-realization constraint, directed/bipartite/hypergraph degree-sequence theorem, counting/sampling of all realizations, edge-weight semantics, or benchmark speedup is claimed.

## Scope

Exactly four paths are intended to differ from the live base:

- `include/algorithms/graphs/graphical_degree_sequence.hpp`;
- `tests/test_graphical_degree_sequence_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_graphical_degree_sequence.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, BFPRT, Count-Sketch, or temporary-file churn is part of this slice. Connector-granularity commits, if any, should be squash-merged so `main` receives one coherent recovery checkpoint.
