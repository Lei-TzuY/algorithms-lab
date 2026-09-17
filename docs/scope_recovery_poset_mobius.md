# Scope recovery: finite-poset zeta transform and Möbius inversion

## Coverage decision

Fresh live-state coverage at `main@c4bb97e627300902bea0e41dc3198154ff5746ba` found no poset zeta transform, Möbius inversion, incidence-algebra API, historical PR, or occupied `mobius` / `poset` branch. The immediately preceding recovery slice is divide-and-conquer optimized partition DP; this checkpoint deliberately changes proof model rather than adding another DP-optimization variant.

The Phase-45--69 compiler/backend history remains prospectively frozen. This slice follows `docs/scope_recovery_after_phase69.md` plus the fresh live coverage audit and does not modify the stale historical `ROADMAP.md`.

## Production contract

`algorithms::combinatorial::FinitePosetIndex` accepts the repository's directed `Graph` as a relation DAG whose reachability defines a finite partial order.

- undirected input rejects;
- self-loops or any directed cycle reject;
- parallel arcs collapse structurally and stored edge weights are ignored;
- the empty directed graph is a valid empty poset;
- a deterministic smallest-vertex-first Kahn order is exposed as one linear extension;
- `less_equal(x,y)` is true exactly when `x == y` or a directed path reaches `y` from `x`;
- `zeta_transform(f)[y] = sum_{x <= y} f[x]` over signed 64-bit values;
- `mobius_invert(g)` returns the unique signed-64 vector `f` whose zeta transform is `g`;
- vector-size and vertex bounds are validated explicitly.

Internal transform sums use a first-principles arbitrary-magnitude signed accumulator built only from standard `uint64_t` limbs. Therefore cancellation such as `INT64_MAX + INT64_MIN` is evaluated exactly rather than depending on a lucky summation order. A public transform entry throws `std::overflow_error` only when its mathematical value is outside `int64_t`.

## Proof obligations

A directed acyclic graph induces a partial order under reflexive reachability. The deterministic topological order is therefore a linear extension. In that order the zeta operator is a unit lower-triangular linear transform: every entry depends on itself and only earlier comparable elements.

The inversion step processes that linear extension in order and solves

`f(y) = g(y) - sum_{x < y} f(x)`.

Because the diagonal coefficient is one, this triangular solve is unique and is exactly Möbius inversion in the incidence algebra of the finite poset. The implementation does not need to materialize the Möbius-coefficient matrix.

The reverse-topological reachability construction is correct because every outgoing neighbor appears later in a linear extension; by the time a vertex is processed, each child's complete upward closure is already known.

These are mathematical proof obligations. Randomized differential testing is implementation evidence, not a proof of the general incidence-algebra theorem.

## Verification

Focused candidate bytes pass:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection/fail-fast: 4/4.

Deterministic evidence covers undirected/cycle/self-loop rejection, empty semantics, a diamond poset, parallel arcs with conflicting ignored weights, chain/antichain behavior, deterministic linear-extension tie breaking, vector/vertex validation, exact full-width cancellation, and true positive/negative representability overflow.

Primary randomized evidence uses 900 fixed-seed directed acyclic multigraphs with 0..12 vertices. The independent oracle computes reflexive reachability with Floyd-Warshall, not reverse-topological propagation. Small signed values make the oracle arithmetic trivially representable; production zeta output must equal the independent full-scan result exactly, every `less_equal` answer must match the closure, every returned order must be a valid linear extension, and Möbius inversion must reconstruct the original vector exactly. Repeated builds must be deterministic.

## Complexity / non-claims

For `V` vertices and `E` distinct structural arcs, construction uses deterministic topological sorting plus reverse closure propagation. The direct byte-matrix baseline is conservatively bounded by `O(VE + V^2)` time and `O(V^2 + E)` storage. Each zeta transform or Möbius inversion scans the materialized order relation in `O(V^2)` time and uses `O(V)` result state beyond the resident index, plus temporary exact-sum limbs proportional to the number of machine words required by one intermediate sum.

This slice does **not** claim:

- a transitive-reduction or Hasse-diagram constructor;
- a total-monotonicity/SMAWK relation;
- subset-lattice fast zeta transforms;
- arbitrary-precision public outputs;
- dynamic poset updates;
- poset dimension, linear-extension counting, incidence-algebra multiplication, or a precomputed Möbius matrix;
- any benchmark or universal performance advantage over sparse specialized transforms.

## Scope

Exactly three new paths are intended:

- `include/algorithms/combinatorial/poset_mobius.hpp`;
- `tests/test_poset_mobius_cases.hpp`;
- `docs/scope_recovery_poset_mobius.md`.

The merged isolated verification architecture automatically enrolls `tests/test_*_cases.hpp`, so no CMake or `tests/test_main.cpp` edit is needed. No README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, occupied recovery surface, or temporary-file churn is included.
