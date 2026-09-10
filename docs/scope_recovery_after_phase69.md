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
