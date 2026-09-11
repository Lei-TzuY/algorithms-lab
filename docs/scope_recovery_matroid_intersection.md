# Scope recovery: unweighted finite matroid intersection

## Why this frontier

After the suffix-automaton checkpoint reached exact merged-main green, a fresh
coverage audit deliberately moved away from the recent string/Mo/Li-Chao/centroid
sequence. Repository history and code search found no matroid or matroid-
intersection capability. This slice adds the classical augmenting exchange-graph
proof model instead of another adjacent string, range-query, tree, or cut variant.

The frozen Phase-45--69 compiler/backend surface remains untouched. Prospective
authority remains the post-Phase-69 recovery decision plus live coverage audits;
the historically presentation-lagged ROADMAP is intentionally unchanged.

## Production contract

`maximum_cardinality_matroid_intersection` works over a finite ground set
`[0,n)` and two caller-supplied independence predicates. Each predicate receives
an increasingly sorted subset of ground-element ids. The result returns one
deterministic maximum-cardinality common independent set together with
augmentation and oracle-call diagnostics.

The caller must supply genuine matroid independence predicates. The implementation
can validate that both predicates accept the empty set and can replay common
independence after every augmentation, but no finite black-box query schedule can
certify the hereditary and exchange axioms for an arbitrary predicate. Those
matroid axioms are therefore an explicit precondition rather than a hidden claim.

## Exchange-graph invariant

Let `I` be the current common independent set.

- a ground element `x` outside `I` is a source when `I + x` is independent in
  the first matroid;
- an outside element is a sink when `I + x` is independent in the second;
- for `y in I`, `x notin I`, an arc `y -> x` exists when
  `I - y + x` is independent in the first matroid;
- the reverse orientation `x -> y` exists when that exchange is independent in
  the second matroid.

BFS from all sources finds a shortest source-to-sink alternating exchange path.
Toggling membership along that path increases `|I|` by exactly one. The standard
matroid-intersection augmenting-path theorem guarantees the toggled set remains
common independent; when no augmenting path exists, `I` has maximum cardinality.
Production additionally replays both independence predicates after every toggle
and fails closed if that contract is violated.

Tests are implementation evidence for this recurrence; they do not replace the
matroid-intersection theorem.

## Verification

Focused repo-style builds passed before upload under GCC strict warnings, Clang
strict warnings, and actual GCC ASan+UBSan, four test groups each.

Deterministic coverage includes:

- empty ground set and invalid/missing oracle contracts;
- a partition-by-partition instance where greedy selection gets stuck at one
  element and the optimum of two requires the alternating path
  `outside -> inside -> outside`;
- repeated execution with the exact same selected-element witness.

Randomized differential evidence includes:

- 1,000 fixed-seed pairs of partition matroids over at most 11 elements;
- 600 fixed-seed graphic-matroid / partition-matroid intersections over at most
  10 ground edges;
- independent exhaustive enumeration of every ground-set subset to compute the
  exact optimum cardinality for each instance;
- replay of both supplied independence predicates on every returned witness.

The exhaustive oracle shares the concrete matroid predicates because those
predicates define the input instance, but it does not use exchange graphs,
augmentation, BFS, or any production recurrence.

## Complexity / non-claims

For ground size `n` and final rank `r`, the direct baseline performs
`O(r n^2)` independence-oracle calls. Because this implementation materializes
an `O(n)` sorted candidate subset before each black-box call, its non-oracle work
is conservatively `O(r n^3)`. If one independence query costs `T`, total time is
`O(r n^3 + r n^2 T)` and auxiliary algorithm state is `O(n)` excluding oracle
internals and temporary candidate vectors.

This slice does not implement weighted matroid intersection, matroid parity,
representation-specific circuit oracles, rank-oracle acceleration, or a dynamic
matroid interface. It is a first-principles maximum-cardinality baseline whose
proof boundary stays explicit.
