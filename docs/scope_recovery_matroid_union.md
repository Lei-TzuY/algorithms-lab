# Scope recovery: maximum-cardinality matroid union

## Coverage decision

A fresh live audit at `main@22e05c56142d528340a2a4ad3039a8b9215d67ae`
found no matroid-union / matroid-partition production API in default-branch code,
PR history, or branches. The repository already contains sealed unweighted and
weighted matroid intersection, but those require one selected set to be
independent in every matroid. Matroid union asks a different question: maximize
a set whose elements can be assigned across several matroids so that each
assigned layer is independent.

The long-lived `scope-recovery-minimum-cycle-basis` branch remains occupied and
is deliberately untouched. The frozen Phase-45--69 compiler/backend surface is
also untouched. Prospective scope remains governed by
`docs/scope_recovery_after_phase69.md` plus this fresh coverage audit.

## Production contract

`maximum_cardinality_matroid_union(ground_size, matroids)` operates on the common
ground set `[0, ground_size)` and returns:

- the sorted maximum-cardinality selected element set;
- one sorted independent subset per supplied matroid;
- an explicit optional matroid assignment for every ground element;
- augmentation count and per-input-oracle call diagnostics.

Zero supplied matroids have rank zero and return the empty selected set. Every
non-empty `MatroidIndependenceOracle` must accept the empty set. General matroid
axioms are a caller precondition because no finite black-box query schedule can
certify hereditary/exchange axioms for an arbitrary predicate. Expanded-ground
size multiplication is checked before allocation.

## Reduction and proof obligation

For `k` input matroids on ground set `E`, production constructs the conceptual
expanded ground set `E x {0,...,k-1}`. It then invokes the repository's sealed
first-principles maximum-cardinality matroid-intersection implementation on two
matroids over those copies:

1. the direct sum of the `k` supplied matroids, where copies assigned to layer
   `i` must form an independent set of matroid `i`;
2. a partition matroid allowing at most one copy of each original element.

Every common-independent copy set therefore maps to a feasible matroid-union
assignment by dropping copy-layer tags. Conversely, every feasible union
assignment lifts to a common-independent copy set of the same cardinality.
Thus maximum-cardinality intersection on the expanded ground set has exactly the
matroid-union optimum cardinality. Returned copy witnesses are mapped back to
original elements and their assigned layers, then every layer is replayed
through its supplied oracle.

This is intentional cross-layer reuse, not a claim that matroid intersection and
matroid union are the same problem. Correctness relies on the standard direct-sum
and partition-matroid reduction plus the already-sealed matroid-intersection
augmenting-path theorem.

## Verification

Focused repo-native verification on the exact live matroid-intersection API
passed before upload under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic evidence covers zero matroids, missing/rejecting oracles, expanded
copy-count overflow, and a two-partition-matroid instance where element-order
first-fit selects only three elements while a cross-layer reassignment reaches
the exact optimum four.

Primary optimization verification does not invoke the production reduction or
matroid-intersection recurrence. It directly enumerates every element state
`unselected / layer-0 / ... / layer-(k-1)`, checks the caller-supplied layer
independence predicates, and records the largest feasible assignment. Production
matches this oracle on 300 fixed-seed random partition-matroid unions with up to
seven elements and three layers.

A second 180-instance corpus uses two graphic matroids on the same random edge
ground set. The oracle again exhaustively assigns every edge to `unselected`,
forest 0, or forest 1, and production must return the exact maximum number of
edges decomposable into two forests. Every production layer, selected element,
and element-to-layer assignment is independently replayed.

The supplied independence predicates are part of the problem definition and are
shared with the exhaustive oracle; the optimization search itself is structurally
independent from copy expansion and matroid intersection.

## Complexity / non-claims

Let `n=|E|`, `k` be the number of supplied matroids, `N=kn` the expanded ground
size, `r` the returned union rank, and `T` an upper bound on one supplied
independence-oracle call. Combining the existing direct intersection baseline
with the direct-sum wrapper gives the conservative code-local bound
`O(r*N^3 + r*N^2*k*T)` time. Resident reduction state is polynomial in `N` plus
caller-oracle state and returned witnesses.

This slice does not claim weighted matroid union, matroid parity, oracle-axiom
validation, rank-oracle/circuit acceleration, or a stronger specialized union
algorithm bound.

## Scope

Exactly four paths change:

- `include/algorithms/combinatorial/matroid_union.hpp`;
- `tests/test_matroid_union_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof/coverage document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, occupied minimum-cycle-basis, or temporary-file surface
is changed.
