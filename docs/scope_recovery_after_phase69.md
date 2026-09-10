# Scope recovery after Phase 69

## Decision

`algorithms-lab` remains an algorithms-and-data-structures foundations laboratory.
The repository keeps the already-merged Phase 45–69 SSA/backend sequence for
historical reproducibility, but that compiler/runtime surface is frozen here and
is no longer an active conquest frontier.

The proposed Phase-70 `return_void` / fixed-frame-exit continuation was reviewed
at exact head `106d118c3bbbc4cc933f327c5d4ad94f1ac8c426`. GitHub Actions run
`34516131510` passed GCC release, Clang release, and GCC ASan+UBSan, but green CI
was not sufficient to override the repository mission. Pull request #157 was
therefore closed without merge. Its branch is retained as extraction material
for a compiler/runtime repository rather than continued inside this lab.

## Why the boundary moved

Directed dominators and dominance frontiers remain legitimate graph-algorithm
subjects. Beginning with Phase 45, however, the sequence moved from graph
analysis into compiler construction: SSA materialization/pruning/destruction,
register allocation, spilling, frame/backend lowering, retained execution
semantics, and bounded backend CFG execution. Those are coherent engineering
capabilities, but they no longer deepen the repository's stated algorithm/data-
structure mission.

This recovery does not destructively rewrite history. Existing merged code and
its tests stay buildable. The governance change is prospective: new work must
again pass the repository-specific test of substantial algorithmic or data-
structural depth.

## Recovered frontier

The first recovered executable slice is exact planar closest-pair search over the
existing bounded-integer `Point2i` geometry domain. It adds a classical
`O(n log n)` divide-and-conquer algorithm, deterministic witness semantics, and
an independent quadratic differential oracle. This is intentionally a return to
algorithmic depth rather than a documentation-only reset.

After that checkpoint reached exact merged-main green, the second recovered
slice advances the same geometry foundation through a different algorithmic
paradigm: exact planar diameter. It reuses the sealed monotone-chain convex hull
and then enumerates antipodal hull pairs with rotating calipers, returning a
canonical farthest-pair witness and verifying the complete result against an
independent quadratic all-pairs oracle.

After the planar-diameter checkpoint also reached exact merged-main green, the
third recovered slice moves from point-set extremal geometry to area aggregation:
exact axis-aligned rectangle union area via an x-sweep, y-coordinate compression,
and a first-principles covered-length segment tree. It adds a genuine sweep-line
paradigm with exact bounded-integer area arithmetic and independent small-grid
union-area verification rather than another distance-query variant.

After the rectangle-union checkpoint reached exact merged-main green, the next
recovered slice leaves the geometry streak rather than farming adjacent polygon
helpers. It targets deterministic exact weighted global minimum cut via the
Stoer-Wagner maximum-adjacency/contraction algorithm. The slice reuses the
existing undirected-capacity edge abstraction, but not max-flow as its production
engine; exhaustive bipartition remains the primary oracle, while the sealed
Gomory-Hu tree supplies secondary cross-implementation evidence. This is a
deliberate contrast among direct deterministic contraction, repeated-flow
cut-equivalent trees, and the sealed randomized unweighted Karger contract.

## Prospective frontier authority

`ROADMAP.md` still contains historical presentation drift from the frozen
compiler/backend excursion (for example an old backend phase can appear as an
active frontier even though later backend phases were already merged and then
prospectively frozen here). That stale heading must not be used to resume
compiler/runtime conquest.

Until the historical roadmap is deliberately normalized without rewriting
repository history, prospective work is governed by this recovery decision plus
a fresh live-state / architecture-coverage audit. New slices must add substantial
algorithmic or data-structural depth, executable behavior, proof obligations,
and independent verification; green CI alone is not a reason to continue an
out-of-scope subsystem.
