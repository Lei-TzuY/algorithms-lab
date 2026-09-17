# Scope recovery: exact Z2 persistent homology on finite filtered complexes

## Coverage decision

A fresh live-state audit at `main@9e649e6dbd641627f39b200d9c9aae6031d8a0db`
found no persistent-homology, homology, barcode, or simplicial-persistence
production surface, historical recovery commit, open PR, or occupied homology
branch. The immediately preceding recovery slice is finite-poset zeta / Möbius
inversion. This checkpoint deliberately changes proof model again: from incidence-
algebra triangular inversion to filtered chain complexes and boundary-matrix
reduction over `Z2`.

The frozen Phase-45--69 compiler/backend history remains out of scope under
`docs/scope_recovery_after_phase69.md`. No historical ROADMAP continuation is
introduced.

## Production contract

`algorithms::topology::persistent_homology_z2` accepts a finite collection of
non-empty simplices with signed 64-bit filtration values.

- simplex vertex order is canonicalized increasingly;
- repeated vertices inside one simplex reject;
- duplicate simplices reject after canonicalization;
- every codimension-one face must be present (downward closure);
- every face filtration must be no later than its coface filtration;
- equal-filtration simplices are ordered by increasing dimension and then
  lexicographically, so every face precedes its coface;
- coefficients are exactly `Z2`, so orientations/signs are irrelevant;
- the empty complex is valid;
- output contains the deterministic canonical simplex order plus every finite or
  essential persistence interval;
- each interval exposes its homological dimension, birth filtration, optional
  death filtration, birth-simplex index, and optional death-simplex index.

Zero-lifetime intervals are retained as genuine reduction pairs. They simply
contribute to no half-open barcode state `[birth, death)` at a filtration
threshold.

## Reduction invariant and proof obligations

Let columns be simplex boundaries in filtration-compatible order. The production
algorithm stores each boundary as a sorted sparse set of row indices and reduces
left-to-right over `Z2`.

For a current column, while its lowest nonzero row is already the pivot of an
earlier reduced column, XOR that earlier column into the current one. Therefore
at termination every nonzero reduced column has a unique pivot row.

- a zero reduced column creates a homology class;
- when a later column receives pivot row `i`, it pairs with the class born at
  simplex `i` and kills it at the later simplex filtration;
- a zero column never used as a later pivot remains an essential interval.

This is the standard persistence pairing for a filtered chain complex over a
field. The implementation additionally checks that every pivot used as a death
partner was previously a birth simplex; violation is treated as an internal
logic error rather than silently producing a malformed barcode.

Random testing is implementation evidence. It is not presented as a proof of the
persistence-reduction theorem.

## Independent verification

Focused candidate bytes pass:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection/fail-fast: 4/4.

Deterministic cases cover malformed/downward-closure/filtration validation,
empty-complex semantics, a filled triangle whose one-dimensional class dies when
the face appears, a tetrahedron boundary with one essential `H2` class, and
repeat-build determinism.

Primary randomized evidence uses 700 fixed-seed downward-closed filtered
simplicial complexes with 0..6 vertices and dimensions through two. The oracle is
not another persistence reduction. At every distinct filtration threshold it:

1. rebuilds the active chain groups;
2. constructs dense `Z2` boundary matrices directly from simplex faces;
3. computes each boundary rank by independent Gaussian elimination; and
4. derives `beta_k = dim C_k - rank(d_k) - rank(d_{k+1})`.

The number of active production barcode intervals in every dimension must equal
those independently computed Betti numbers at every threshold. Repeated
production runs must return byte-for-byte equivalent canonical simplex order and
interval records.

## Complexity / non-claims

For `N` simplices, the direct sparse-vector column-reduction baseline is
conservatively `O(N^3)` time and `O(N^2)` reduction storage in the worst case,
plus map-based simplex lookup/validation overhead. It is intentionally a clear
first-principles correctness baseline rather than a high-performance TDA engine.

This slice does **not** claim:

- integer-coefficient homology, torsion, or arbitrary coefficient fields;
- Vietoris--Rips / Cech / alpha-complex construction from point clouds;
- cohomology, representative-cycle reconstruction, clearing/compression,
  chunking, spectral sequences, zigzag persistence, or multiparameter
  persistence;
- stability/interleaving theorems or geometric-noise guarantees;
- compressed/sparse asymptotic optimality or benchmark superiority.

## Scope

Exactly three new paths are intended:

- `include/algorithms/topology/persistent_homology.hpp`;
- `tests/test_persistent_homology_cases.hpp`;
- `docs/scope_recovery_persistent_homology.md`.

The sealed isolated recovery-test architecture automatically enrolls
`tests/test_*_cases.hpp`, so no CMake, historical ROADMAP, README, workflow,
benchmark, frozen compiler/backend, or temporary-file churn is required.
