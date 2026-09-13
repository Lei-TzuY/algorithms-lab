# Scope recovery: bounded exact Tutte polynomial

## Coverage decision

Fresh live coverage at `main@0a2d305170e965850e3c41babcd1f7d53d50cb53`
found no Tutte-polynomial / chromatic-polynomial production surface in default-
branch code, pull-request history, or the branch namespace. The prospective
authority remains `docs/scope_recovery_after_phase69.md`; the historical ROADMAP
is not extended. The immediately preceding merged slice is bounded exact directed
feedback vertex set, so this checkpoint deliberately changes proof model from
cycle-hitting search to a graph-invariant deletion/contraction recurrence.

## Production contract

`exact_tutte_polynomial(graph)` accepts the repository's undirected multigraph
abstraction and returns a sparse coefficient table for the ordinary unweighted
Tutte polynomial.

- directed graphs are rejected;
- parallel logical edge copies remain distinct;
- self-loops retain their ordinary Tutte loop semantics;
- stored edge weights are intentionally ignored;
- isolated vertices are allowed and do not change the polynomial;
- output terms are deterministic and sorted by `(x_degree, y_degree)`;
- at most 20 logical edges are accepted; larger instances throw
  `std::length_error` rather than hiding exponential work;
- diagnostics expose the logical input edge count and number of memoized states
  actually evaluated.

Coefficients are represented as `uint64_t`. Under the explicit 20-edge bound this
is exact rather than a best-effort narrowing: Tutte coefficients are
non-negative and `T_G(2,2)=2^|E|`, so every individual coefficient is at most
`2^20`. Additions are nevertheless checked fail-closed.

## Algorithm and proof boundary

Production uses the classical deletion-contraction identities on one
deterministically selected logical edge `e`:

- loop: `T_G = y T_{G-e}`;
- bridge: `T_G = x T_{G/e}`;
- otherwise: `T_G = T_{G-e} + T_{G/e}`.

Bridge status is decided directly in the current multigraph state by removing
only the selected edge copy and testing whether its endpoints remain connected;
parallel copies therefore prevent a false bridge classification. Contraction
keeps parallel copies and naturally turns formerly parallel edges into loops.
Canonical sorted edge states are memoized.

Correctness relies on the classical Tutte deletion-contraction characterization
and its loop/bridge base identities. Finite tests are implementation evidence,
not a proof of that theorem.

## Independent verification

The primary randomized oracle is structurally independent from deletion-
contraction. For each edge subset `A`, tests compute the graphic-matroid rank
`r(A)` with an independent DSU and expand

`(x-1)^(r(E)-r(A)) (y-1)^(|A|-r(A))`

coefficient-by-coefficient through the binomial theorem. Summing those expansions
over every subset yields the complete oracle coefficient table.

Committed focused evidence covers:

- empty and isolated-vertex graphs;
- one bridge (`x`) and one loop (`y`);
- parallel edges (`x+y`);
- a triangle (`x^2+x+y`);
- multiplicativity across a bridge component plus loop component (`xy`);
- directed rejection and explicit 20-edge bound rejection;
- ignored signed weights, insertion-order invariance, and deterministic replay;
- 500 fixed-seed random undirected multigraphs with `0..6` vertices and at most
  ten logical edge copies, requiring exact coefficient-vector equality with the
  independent subset-rank oracle;
- `T(2,2)=2^|E|` on every randomized instance;
- on connected randomized instances, `T(1,1)` additionally equals the sealed
  Kirchhoff spanning-tree count modulo `1,000,000,007` as secondary cross-layer
  evidence.

The final focused candidate passes repository strict-warning GCC, strict-warning
Clang, and actual GCC ASan+UBSan, four test groups in each configuration.

## Complexity and non-claims

With `m <= 20` logical edges and `n` vertices, the direct memoized recurrence has
at most exponentially many canonical edge states. Bridge testing is `O(n+m)` per
new state and canonical edge sorting contributes `O(m log m)`, so a conservative
bound is `O(2^m * (n + m log m))` time and `O(2^m * m)` memoized edge-state /
polynomial payload scale. Recursion depth is `O(m)`.

This is a bounded exact educational baseline. It does not claim a polynomial-
time Tutte algorithm, arbitrary-precision coefficients, weighted/random-cluster
polynomials, symbolic factorization, planar-dual acceleration, chromatic/
reliability polynomial APIs, or large-instance practicality.
