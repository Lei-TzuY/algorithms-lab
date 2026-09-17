# Scope recovery: filtration-preserving elementary simplicial collapse

## Coverage decision

The live recovery tree already contains exact `Z2` persistent homology over finite
filtered simplicial complexes, but no simplicial-collapse, coreduction, or discrete-
Morse preprocessing surface. This checkpoint adds a topology-preserving reduction
layer rather than another barcode algorithm.

The public contract deliberately targets **elementary codimension-one simplicial
collapse**. A candidate free face is removed only when it has exactly one active
immediate coface and the face/coface filtration values are equal. The equal-
filtration condition is essential: without it a sublevel can contain the face before
the coface appears, so deleting the pair need not preserve the filtered persistence
module.

## Production contract

`algorithms::topology::filtration_preserving_elementary_collapse` accepts the same
`FilteredSimplex` representation as the persistent-homology recovery surface.

- simplex vertices are canonicalized increasingly;
- empty simplices, repeated vertices, duplicate simplices, missing codimension-one
  faces, and decreasing face/coface filtration reject;
- pair selection is deterministic: active faces are considered in increasing
  dimension and lexicographic vertex order;
- a pair `(tau, sigma)` is eligible only when `sigma` is the unique active immediate
  coface of `tau` and both filtration values are equal;
- both simplices are removed atomically and the process repeats to a fixed point;
- output includes the reduced filtered complex and the ordered collapse-pair witness;
- the reduced complex is returned in filtration/dimension/lexicographic order.

The implementation does not call persistent homology to choose pairs or to decide
correctness. Persistent homology is used only as a cross-layer integration oracle in
tests.

## Invariant / proof obligation

At each step `tau` has exactly one active codimension-one coface `sigma`. In a
downward-closed simplicial complex this implies that no larger active simplex can
contain `tau`: any such simplex would force another active immediate coface of
`tau`. Removing `tau` and `sigma` therefore leaves a simplicial complex and is an
elementary collapse, hence a homotopy equivalence.

Because `filtration(tau) == filtration(sigma)`, every filtration threshold either
contains neither member or contains the whole elementary-collapse pair. The same
collapse witness consequently restricts to valid elementary collapses on every
sublevel where the pair exists. Betti numbers and the represented persistence module
are preserved.

The repository's persistent-homology reducer intentionally retains zero-lifetime
`[t,t)` reduction pairs. An equal-filtration elementary collapse may remove exactly
such an ephemeral pair. Therefore this checkpoint does **not** claim byte-for-byte
identity of raw reduction pairings. Cross-integration compares essential and
positive-lifetime barcode intervals, while the independent primary oracle compares
Betti numbers at every filtration threshold.

## Verification

Focused candidate bytes pass:

- GCC C++20 repository strict warnings-as-errors;
- Clang C++20 repository strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection/fail-fast.

Deterministic cases cover invalid complexes, a delayed edge that must not collapse,
a same-level edge, a filled triangle that collapses to one vertex, a circle and a
tetrahedron boundary with no free pair, and an explicit zero-lifetime barcode pair.

Randomized evidence uses 500 fixed-seed downward-closed filtered complexes on 0..6
vertices with dimensions through three. Every returned collapse sequence is replayed
against an independent active-set model; each pair must be equal-filtration,
codimension one, and uniquely free at the moment of removal. For every distinct
filtration threshold, before/after Betti vectors are independently recomputed from
dense `Z2` boundary matrices using Gaussian rank. In addition, positive-lifetime
and essential intervals from the existing persistent-homology surface must agree
before and after reduction. Repeated production runs must return identical reduced
complexes and collapse witnesses.

Random tests are evidence, not the proof of the elementary-collapse theorem.

## Complexity / non-claims

The direct correctness baseline precomputes immediate-coface incidence, then scans
all active simplices after each successful removal. With `N` simplices and `I`
immediate incidences, time is `O(N * (N + I))`, conservatively `O(N^3)` when
`I = O(N^2)`, with `O(N + I)` auxiliary storage plus the returned witness.

This slice does not claim discrete-Morse optimality, minimum-size cores, persistence-
aware aggressive reduction beyond equal-filtration elementary pairs, representative
cycle reconstruction, geometric-complex construction, or production TDA speedups.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/topology/filtered_simplicial_collapse.hpp`;
- `tests/test_filtered_simplicial_collapse_cases.hpp`;
- `docs/scope_recovery_filtered_simplicial_collapse.md`.

The isolated recovery-test architecture auto-enrolls the case header, so no CMake,
ROADMAP, README, workflow, benchmark, frozen compiler/backend, or temporary-file
churn is required.
