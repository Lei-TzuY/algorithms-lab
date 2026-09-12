# Scope recovery: FIFO push-relabel maximum flow

## Coverage decision

A fresh live-state audit at `main@d932f95d8349171d92deb387af1928d659a1de9c`
found the sealed Dinic maximum-flow implementation but no push-relabel API or
production implementation on the default branch. PR-history search likewise found
no merged push-relabel slice. The branch named `scope-recovery-push-relabel` is a
stale reservation: its head is the already-merged incremental-DAG-ordering commit
`e489b92f...`, and it is `ahead 0 / behind 38` relative to this base. The genuinely
occupied `scope-recovery-minimum-cycle-basis` surface is left untouched.

Prospective authority remains `docs/scope_recovery_after_phase69.md`; the frozen
Phase-45--69 compiler/backend history and stale historical ROADMAP frontier are not
modified.

## Production contract

`push_relabel_max_flow` is an independent first-principles maximum-flow baseline
beside the sealed Dinic implementation. It accepts the same `CapacityEdge` input
contract and returns the same `MaxFlowResult` certificate:

- source and sink must be distinct valid vertices;
- capacities must be non-negative signed-64 values;
- self-loops, parallel edges, antiparallel edges, and zero-capacity edges are
  supported;
- result edges preserve input order and expose a feasible per-edge flow;
- the final residual source-side set is returned as an explicit minimum-cut
  witness;
- a mathematically valid maximum flow larger than `INT64_MAX` is rejected rather
  than narrowed or wrapped.

Self-loops are retained in the result with flow zero and do not materialize a
residual pair. Temporary preflow excess is accumulated in a two-limb unsigned
counter so an intermediate excess larger than `UINT64_MAX` does not incorrectly
reject a final maximum flow that is still representable as signed 64-bit.

## Algorithm and invariants

Production implements the FIFO push-relabel preflow algorithm with current-arc
optimization and no gap/global-relabel heuristic.

1. `height[source] = V`, all other initial heights are zero, and every positive
   residual arc leaving the source is saturated to create a preflow.
2. Every queued non-terminal vertex has positive excess.
3. A push is admissible only when `height[u] == height[v] + 1`; it transfers the
   minimum of the vertex excess and residual capacity while updating the paired
   reverse residual arc.
4. If an active vertex has no remaining admissible arc, relabel sets its height
   to one plus the minimum height of a residual neighbor and resets its current
   arc.
5. Discharge continues until the vertex has zero excess. On termination every
   non-terminal vertex therefore satisfies flow conservation.
6. No residual source-to-sink path may remain. Reachability in the final residual
   graph yields a cut whose capacity must exactly equal the returned flow value.

Correctness uses the classical push-relabel preflow invariants and max-flow/min-cut
theorem; testing is implementation evidence rather than a replacement for those
proofs.

## Verification

The exact focused candidate passed before upload under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

A subsequent code-review gate found and fixed a representability bug in the
initial candidate: three `INT64_MAX` source arcs entering one dead-end vertex can
create a temporary excess greater than `UINT64_MAX` even though the true maximum
flow is only one. The regression now requires that case to return flow one rather
than throw, while the existing non-representable final-flow test must still fail
closed.

Deterministic cases cover the classical 23-unit network, malformed endpoints,
negative capacities, source/sink equality, parallel and antiparallel edges,
self-loops, zero capacities, disconnected/dead-end preflows that must return to
the source, exact `INT64_MAX` flow, wide temporary preflow with a narrow optimum,
and non-representable aggregate-flow rejection.

Primary randomized oracle: 700 fixed-seed directed multigraphs with 2..8 vertices,
0..39 logical edges, and capacities in `[0,8]`. Every internal-vertex bipartition
is enumerated independently to obtain the exact minimum s-t cut. Production must
match that optimum. Every returned edge is replayed for capacity bounds and flow
conservation, and the returned source-side cut is replayed for exact capacity.

As secondary cross-layer evidence, every randomized result is also compared with
the already-sealed Dinic implementation. Dinic is not the primary correctness
oracle.

## Complexity and non-claims

This direct FIFO/current-arc educational implementation conservatively claims
`O(V^2 E)` worst-case time and `O(V+E)` resident algorithm storage. It deliberately
does not claim the stronger practical behavior of gap heuristics, global relabel,
highest-label selection, dynamic trees, or benchmark speedups over Dinic.

## Scope

Exactly four repository paths change:

- `include/algorithms/graphs/push_relabel_max_flow.hpp`;
- `tests/test_push_relabel_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused recovery proof document.

No CMake, README, historical ROADMAP, scope-recovery authority, workflow,
benchmark, frozen compiler/backend, occupied minimum-cycle-basis surface, or
temporary-file churn is part of this slice.