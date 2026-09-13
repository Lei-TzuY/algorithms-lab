# Scope recovery: exact Burnside orbit counting

## Coverage decision

A fresh live audit at `main@2d7fbff5fbd693b7b507cd03c30d09e36a5c2aeb`
found no Burnside/Pólya/orbit-counting implementation in default-branch code,
commit history, or the branch namespace. The audit deliberately avoided occupied
recovery surfaces such as numerical eigensolvers, alternative hashing schemes,
Delaunay/suffix-automaton work, matching certificates, and the recent bounded
exact counting slices.

This slice adds a different proof model: a finite permutation group acts on
position colorings, every group element contributes a fixed-point count obtained
from its cycle decomposition, and the exact number of orbits is the average of
those fixed-point counts. The primary verification oracle explicitly enumerates
colorings and materializes their group orbits instead of reusing Burnside's
formula.

Prospective authority remains `docs/scope_recovery_after_phase69.md`; the frozen
compiler/backend excursion and stale historical ROADMAP do not drive this work.

## Production contract

`algorithms::combinatorial::count_color_orbits_burnside` accepts:

- an explicit number of positions;
- a positive number of colors;
- every element of a finite permutation group on those positions.

The group description is validated before counting:

- the group is non-empty;
- every element has the requested degree and is a bijection;
- duplicate elements are rejected;
- the identity permutation is present;
- every ordered pair of supplied elements composes to another supplied element.

The returned result contains the exact `uint64_t` orbit count, the exact summed
fixed-coloring numerator, and one replayable diagnostic per input group element:
its cycle count and the corresponding fixed-coloring count.

The empty position set is supported when the explicit group is `{identity}`; it
has one coloring for every positive color count. A zero color count is rejected
rather than introducing a special `0^0` convention.

## Burnside invariant

For a permutation `g`, a coloring is fixed by `g` exactly when every cycle of
`g` is monochromatic. Therefore, if `c(g)` is the number of cycles and `q` is
the color count, the number of fixed colorings is exactly `q^c(g)`.

Burnside's lemma then gives

`number_of_orbits = (sum_g q^c(g)) / |G|`.

Production computes each power and the fixed-point sum with explicit checked
`uint64_t` arithmetic. If an intermediate exact value is not representable, the
call throws `std::overflow_error`; it does not wrap and does not claim arbitrary-
precision support. Divisibility by the validated group order is checked as an
internal theorem/invariant guard.

## Complexity

For `m = |G|` group elements and `n` positions:

- permutation/bijection validation: `O(m n)`;
- closure validation: `O(m^2 n log m)` with ordered-set membership in this
  direct implementation;
- cycle decomposition and fixed-count evaluation: `O(m n)` plus checked integer
  exponentiation implemented as `O(n)` repeated multiplication per element;
- resident auxiliary storage: `O(m n)` for the validated ordered set plus `O(n)`
  traversal state.

This baseline expects an explicitly enumerated finite group. It does not claim a
group-generator closure algorithm, symbolic cycle-index polynomial, arbitrary-
precision counting, or efficient handling of exponentially large groups.

## Verification

Focused exact-candidate verification passed:

- GCC C++20 repository strict warnings-as-errors: 5/5;
- Clang C++20 repository strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with leak detection / halt-on-error: 5/5.

Deterministic cases cover cyclic necklace counting, dihedral bracelet counting,
trivial actions, the empty-position identity action, malformed permutations,
duplicate elements, missing identity, non-closure, zero colors, and arithmetic
overflow.

The primary randomized oracle independently enumerates every coloring for 300
fixed-seed cyclic/dihedral actions with one to six positions and one to three
colors. It explicitly applies every permutation, inserts complete transformed
colorings into orbit sets, and counts orbit representatives. It shares neither
cycle decomposition nor Burnside averaging with production.

Tests provide implementation evidence; they do not replace the mathematical
proof of Burnside's lemma.

## Scope

Exactly four paths are intended to differ from live main:

- `include/algorithms/combinatorial/burnside_orbit_count.hpp`;
- `tests/test_burnside_orbit_count_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, occupied recovery surface, or temporary-file churn is
part of this slice.
