# Scope recovery: exact Levenshtein BK-tree

## Coverage decision

A fresh live audit at `main@4bd79f36cf87e1709270f9d06f7ff7758e2d8694`
after the merged first-principles Karatsuba checkpoint found no BK-tree,
Burkhard-Keller metric tree, or equivalent metric-partition index in default-
branch code. The exact base push CI run `34955761110` completed successfully on
GCC release, Clang release, and GCC ASan+UBSan with zero open pull requests and
zero open issues before promotion. A historical KD-tree branch discovered during
the same audit was already merged as PR #179, so that spatial-index candidate was
explicitly abandoned rather than duplicated.

This slice deliberately changes proof model again. Karatsuba is recursive exact
arithmetic; the BK-tree is a metric-partition structure whose pruning correctness
depends on the triangle inequality. Production reuses the sealed byte-oriented
Levenshtein implementation instead of duplicating its dynamic program.

## Production contract

`BkTreeStringSet` is an incrementally built unique-string set over arbitrary byte
strings:

- `insert(value)` stores a new byte string and returns false for an exact
  duplicate;
- every parent-to-child edge is labelled by the exact Levenshtein distance from
  the parent value to that child value;
- `query_within(query, radius)` returns every resident value whose exact edit
  distance is at most `radius`;
- matches are deterministically sorted by `(distance, unsigned-byte lexical
  value)` independent of traversal order;
- the result exposes visited-node and pruned-child counts as structural
  diagnostics, not performance measurements;
- embedded NUL and high-bit bytes are ordinary data;
- `valid_structure()` is an intentionally expensive audit helper that replays
  reachability, explicit edge labels, and the complete insertion-routing path for
  every resident value.

Duplicate insertion is a no-op because Levenshtein distance zero is exactly byte
string identity. No deletion or mutable-key operation is provided.

## BK-tree pruning invariant

At a node `p`, insertion computes `d(p,x)` and follows the unique child edge with
that label if it exists. Consequently every resident value in a first-level child
subtree labelled `e` has distance `e` from `p` at the routing decision that put it
under that child.

For query value `q`, let `x = d(p,q)` and let the requested radius be `r`. If a
candidate value `v` can satisfy `d(v,q) <= r`, the metric triangle inequality
implies

`|d(p,v) - d(p,q)| <= d(v,q) <= r`.

Therefore only child labels in `[x-r, x+r]` can contain a qualifying value.
Production uses saturating unsigned arithmetic for the upper endpoint so even a
caller-supplied maximum radius cannot wrap the pruning interval.

This metric theorem is the proof obligation. Tests demonstrate implementation
agreement with independent exhaustive scanning; they do not infer the triangle
inequality from samples.

## Verification

The focused final candidate passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan fail-fast execution: 4/4.

Deterministic evidence covers empty trees, duplicate insertion, arbitrary bytes,
repeat determinism, an observable exact-radius pruning case, incremental insert
replay, and broad-radius traversal.

The primary randomized oracle is structurally independent from production. Across
400 fixed-seed trees with up to 35 insertion attempts, each tree executes 30
random queries over an arbitrary-byte alphabet and radii 0..4. The oracle scans
every unique resident string and computes Levenshtein distance with an independent
two-row recurrence rather than the repository's full-table/reconstruction
implementation. Match vectors must agree exactly, and `valid_structure()` is
replayed for every tree.

## Complexity and non-claims

Let `D(a,b)` denote the cost of one sealed Levenshtein evaluation; the current
reused implementation is `O(|a||b|)` time and space because it also reconstructs
an edit script. BK insertion performs one metric evaluation per node on its route.
A radius query evaluates each visited node and is `O(n)` metric evaluations in the
worst case. Resident BK topology is `O(n)` plus owned string storage and child-map
entries. The expensive `valid_structure()` diagnostic intentionally performs many
additional metric evaluations and is not a query-time complexity claim.

This slice makes no expected logarithmic query-time claim, no benchmark-backed
speedup claim, and no claim that edit-script reconstruction is optimal for a
metric-only index. It does not add deletion, nearest-k, arbitrary caller metrics,
Unicode normalization, approximate distance, persistence, concurrency, or
external metric-tree libraries.

## Scope

Exactly four paths should change:

- `include/algorithms/data_structures/bk_tree.hpp`;
- `tests/test_bk_tree_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_bk_tree.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, Karatsuba, or temporary-file churn belongs in this
checkpoint. Connector-generated file commits should be squash-merged so `main`
receives one coherent scope-recovery checkpoint.
