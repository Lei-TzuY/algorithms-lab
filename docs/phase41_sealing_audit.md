# Phase 41 sealing audit — Gomory-Hu all-pairs minimum-cut trees

## Integrated checkpoint

Phase 41 reached merged `main` at `cf9108941293d025ee7124dc85457db6a2c054a6`. The post-merge CI run `34354253443` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

The sealed capability is an exact Gomory-Hu cut-equivalent tree for undirected non-negative-capacity multigraphs. Construction reuses the sealed Dinic implementation for exactly `V-1` s-t minimum-cut computations and exposes the deterministic parent/cut-value tree plus exact pairwise minimum-cut queries.

## Correctness and integration audit

- The standard Gomory-Hu parent/reparent/swap recurrence is implemented directly rather than replaced by a heuristic approximation.
- Undirected capacities are reduced to two directed arcs per non-self-loop edge; exactly one direction crosses any oriented cut, so Dinic's cut value matches the undirected cut value.
- Parallel edges retain multiplicity, zero-capacity edges are valid, self-loops remain cut-neutral, and negative capacities or invalid endpoints are rejected.
- Dinic representability failures propagate; the cut-tree layer does not wrap or saturate capacities.
- A fixed-seed suite checks every vertex pair on 400 small multigraphs against an independent exhaustive s-t bipartition oracle. The oracle does not call Dinic and does not reproduce the Gomory-Hu recurrence.
- The public query baseline intentionally remains `O(V)` time with `O(V)` temporary workspace. No binary-lifting or hidden asymptotic query improvement is claimed.

No unresolved correctness, integration, sanitizer, or claim blocker remains.

## Why Phase 41 stops here

Binary-lifting the already-correct cut tree would improve repeated query latency but would not add a new algorithmic proof boundary. Continuing the phase with query-index variants would therefore be low-value optimization farming rather than a new coherent algorithmic capability.

## Promotion

The next frontier is **Phase 42 — directed minimum arborescence / Chu-Liu-Edmonds**. The repository already has sealed undirected minimum spanning forests, but not the directed rooted analogue. The promoted slice must add directed reachability preconditions, minimum incoming-edge selection, directed-cycle contraction/expansion, a reconstructable rooted arborescence witness, checked signed total cost, and an independent exhaustive small-graph oracle. The Chu-Liu-Edmonds theorem/cycle-contraction argument remains a proof obligation rather than a test-derived claim.
