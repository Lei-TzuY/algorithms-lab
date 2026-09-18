# Scope recovery: persistent cardinality-measured finger tree

## Coverage decision

A fresh post-Phase-69 live audit at `main@6a22c157fc908b1d3d1b99c26da04ee4c3bfff71`
found no finger-tree implementation, commit, pull request, or occupied finger-tree
branch. The sealed persistent byte rope explicitly states that it does not claim
finger-tree bounds. A stale `scope-recovery-series-parallel` branch is genuinely
ahead of main and was therefore left untouched rather than competing with an
occupied surface.

The immediately preceding recovery slice is deterministic SLR parser generation.
This checkpoint deliberately changes proof model from viable-prefix automata and
FIRST/FOLLOW fixed points to a persistent self-adjusting sequence structure with
bounded digits, recursive 2/3 nodes, and amortized end-update analysis. The
prospective authority remains `docs/scope_recovery_after_phase69.md`; the stale
historical compiler/backend ROADMAP is not modified.

## Production contract

`algorithms::data_structures::PersistentFingerTree` is an immutable sequence of
signed 64-bit values. Every update returns a new value while old versions remain
readable and unchanged.

The public surface provides:

- `push_front` / `push_back`;
- `pop_front` / `pop_back` with explicit empty-sequence rejection;
- `front` / `back`;
- zero-based `at(index)`;
- `size`, `empty`, and a deterministic `to_vector` snapshot;
- `valid_invariants()` as a structural diagnostic.

This first recovery slice fixes the measure to leaf cardinality. It deliberately
does not claim generic monoid measures, split/search by arbitrary measures,
concatenation, mutable iterators, allocator guarantees, thread safety, lock-free
behavior, or a standard-library replacement.

## Structural invariant

A tree at level `h` stores elements of exact height `h`.

- level-zero elements are leaves with measure one;
- an internal element has exactly two or three same-height children, height one
  larger than its children, and measure equal to the checked sum of child
  measures;
- a deep tree has non-empty prefix and suffix digits of one through four elements;
- its recursive middle tree is exactly one level higher and therefore stores
  2/3-nodes built from current-level elements;
- every stored tree measure is the checked sum of its prefix, middle, and suffix
  measures.

When a prefix digit overflows, three old elements are promoted into one 3-node
and recursively prepended to the middle tree; suffix overflow is symmetric.
During a pop, an exhausted one-element digit borrows one 2/3-node from the middle
and expands its children back into the exposed digit. If the middle is empty, the
opposite digit collapses directly to a shallower tree.

These transformations preserve sequence order and the 1..4 digit / 2..3 node
shape invariants while reusing every unaffected immutable subtree.

## Persistence and measure lookup

All tree/element nodes are immutable `shared_ptr<const ...>` objects. Updating a
version path-copies only the touched spine; it never mutates a historical node.
Consequently branching version DAGs remain valid simultaneously.

`at(index)` descends by cardinality measure: subtract complete prefix elements,
then the middle measure, then suffix elements; entering a 2/3-node repeats the
same measure-guided descent among its children. The represented order is never
reconstructed through a hidden mutable cache.

## Complexity boundary

For `n` leaves, recursive 2/3 grouping gives `O(log n)` structural depth.
Therefore:

- `at`, `front`, and `back` are `O(log n)` in this direct implementation
  (`front/back` reuse measure lookup rather than adding a separate cached path);
- one end push/pop is worst-case `O(log n)` when digit overflow/underflow carries
  through every level;
- along one ordinary linear update history, the classical bounded-digit
  potential argument gives amortized `O(1)` structural rebalancing for end
  pushes/pops;
- `to_vector` and `valid_invariants` are explicit `O(n)` diagnostics;
- one version stores `O(n)` reachable nodes, while unchanged subtrees are shared
  between versions.

No global amortized `O(1)` claim is made across an adversarial *branching*
persistent history that repeatedly revisits an expensive old version: persistence
can intentionally replay the same worst-case carry from multiple branches. No
worst-case constant-time end-update claim is made.

## Verification

Focused verification uses the repository strict warning flags under GCC and
Clang plus an actual ASan+UBSan build.

Deterministic regressions cover empty rejection, full-width signed values,
front/back insertion, persistent pop semantics, and 2048-element carry/borrow
chains through multiple recursive levels.

Two independent sequence oracles use ordinary `std::vector<int64_t>` state:

- a 1,500-step fixed-seed **branching version DAG** repeatedly selects arbitrary
  historical versions, derives new versions, and verifies both the unchanged
  source snapshot and the new snapshot;
- a 8,000-step fixed-seed linear mixed trace exercises both ends plus indexed
  lookup while checking structure/size after every operation and full snapshots
  periodically.

The vector oracle checks sequence semantics only; it does not duplicate the
finger-tree digit/node recurrence. Tests are implementation evidence, not a
proof of the classical amortized finger-tree argument.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/data_structures/finger_tree.hpp`;
- `tests/test_finger_tree_cases.hpp`;
- `docs/scope_recovery_finger_tree.md`.

The sealed isolated recovery-test architecture auto-enrolls the case header after
CMake reconfiguration, so no CMake, `tests/test_main.cpp`, historical ROADMAP,
README, workflow, benchmark, frozen compiler/backend, occupied recovery surface,
or temporary-file churn is required.
