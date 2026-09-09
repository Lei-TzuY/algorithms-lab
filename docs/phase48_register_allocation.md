# Phase 48 — phi-free virtual-register allocation

## Scope and contract

Phase 48 consumes the sealed Phase-47 `OutOfSsaProgram` and adds the first
register-allocation layer over its retained `SsaValue` virtual registers and
parallel-copy temporaries.

The public result exposes four separate witnesses rather than hiding allocation
behind a final register vector:

- the canonical virtual-location universe;
- exact block and per-operation may-liveness;
- a reconstructable undirected interference edge list;
- one deterministic physical-register assignment per virtual location plus an
  explicit spill list for locations that cannot be colored within the supplied
  register budget.

This is allocation analysis and bounded greedy coloring only. It does not rewrite
the program, insert spill loads/stores, coalesce copies, claim optimal coloring or
minimum spills, model MemorySSA, or perform optimization passes.

## Phi-free operation order

The sealed Phase-47 block execution order is preserved exactly:

1. `entry_moves`;
2. ordinary non-phi `instructions`;
3. `exit_moves`.

Every move has one source use and one destination definition. Ordinary
instructions expose their existing `uses` and optional `definition`. This flattened
operation order is the only instruction-level order used by Phase 48.

The canonical location universe contains every version-zero initial `SsaValue`,
every declared out-of-SSA temporary, and every value/temporary referenced by a
reachable operation. Value locations sort before temporaries; values sort by
`(variable, version)` and temporaries by id. Dead but declared locations remain
visible so the assignment/spill result covers the entire phi-free virtual-location
namespace rather than only currently live values.

## Exact may-liveness invariant

For every reachable lowered block `b`, production solves the standard backward
may-liveness fixed point:

`live_out[b] = union(live_in[s])` over reachable successors `s`

`live_in[b] = use[b] union (live_out[b] - def[b])`

where `use[b]` means locations read before any definition of that location in the
flattened block and `def[b]` means locations defined by the block.

After the block fixed point converges, each block is walked backward operation by
operation. If `L_after` is the live set immediately after an operation,

`L_before = uses union (L_after - definitions)`.

The result materializes both sets for every operation and the block entry/exit
sets. Parallel physical CFG arcs do not duplicate liveness because successor
union is a set operation; they do not change the existence of a future use path.
Unreachable blocks remain explicitly outside the liveness domain.

## Independent forward oracle

Randomized verification does not reproduce the backward recurrence. For each
program-point boundary and each virtual location, the test performs a forward BFS
through the flattened control-flow program. The location is live at that point if
some reachable future path encounters a use before a definition kills the old
value on that path.

The oracle therefore answers the semantic path-to-use question directly. Block
`live_in/live_out` and every operation `live_before/live_after` are compared to
this forward result on fixed-seed graphs containing branches, joins, loops,
backedges, parallel arcs, entry/exit moves, SSA values, and temporary locations.

## Interference invariant

Two distinct locations interfere exactly when they occur simultaneously in a
materialized live boundary set. Production adds an undirected edge for every pair
present together in any:

- block `live_in`;
- block `live_out`;
- operation `live_before`;
- operation `live_after`.

Edges are canonicalized and sorted by virtual-location order. Tests independently
rebuild this graph from the forward path-to-use oracle and require exact edge-set
equality.

A move source that dies at that move is not forced to interfere with the move
destination merely because the two names appear in the same operation. This is
intentional: their values are not simultaneously live after the copy and may
therefore share a physical register. Phase 48 still performs no explicit copy
coalescing; this behavior follows only from the liveness relation.

## Deterministic bounded coloring and spills

The allocator colors locations in descending interference degree, breaking ties
by canonical location order. For each location it selects the lowest physical
register not already used by an assigned neighbor. If every register in
`[0, register_budget)` is blocked, the location remains unassigned and is listed
in `spills`.

`register_budget == 0` is valid and spills every location. The algorithm is a
deterministic greedy coloring heuristic: no optimal-coloring, chromatic-number,
minimum-spill, or spill-cost claim is made.

The executable postcondition is narrower and exact: for every interference edge,
if both endpoints are assigned, their physical-register ids differ.

## Validation

Phase 48 validates the structural assumptions it directly consumes:

- the lowered CFG is directed and `start` is valid;
- lowered block count matches the CFG;
- block `reachable` flags equal actual start reachability;
- unreachable blocks contain no executable payload;
- version-zero initial values cover exactly the declared variable count;
- every value reference has a valid variable id;
- every temporary reference is below `temporary_count`.

It intentionally does not re-prove the entire Phase-47 SSA-destruction provenance,
critical-edge placement, or simultaneous-copy scheduling theorem. Those properties
belong to the sealed producer contract.

## Verification evidence

Deterministic regressions cover:

- three simultaneously live values producing a triangle interference graph and a
  deterministic spill under a two-register budget;
- repeated allocation determinism;
- entry moves through an out-of-SSA temporary and the temporary's exact kill point;
- branch/loop fixed-point propagation;
- undirected CFG rejection, reachability mismatch rejection, and invalid temporary
  references.

The randomized corpus uses 260 fixed-seed reachable directed CFGs with 1–6 blocks,
2–5 variables, a guaranteed reachability chain plus random extra/back edges,
random instructions, entry/exit moves, two temporaries, and register budgets from
zero through four. Every program-point liveness set is compared with the
independent forward path-to-use BFS oracle. The interference graph is rebuilt
independently from those oracle sets, and every assigned interfering pair is
checked for a different register.

Before upload, the focused candidate passes strict GCC, strict Clang, and real
GCC ASan+UBSan builds, 5/5 tests in each configuration. Repository-wide
GCC/Clang/ASan CI on the exact PR head remains the authoritative integration gate.

## Complexity boundary

Let `B` be lowered blocks, `E` CFG arcs, `Q` flattened operations, `L` virtual
locations, and `I` interference edges.

This first-principles implementation deliberately uses dense byte liveness vectors
and a simple repeated global fixed-point pass. A pass scans CFG/dataflow state in
roughly `O((E + Q + B) * L)` work; convergence is finite and monotone, but Phase 48
does not claim an optimized worklist or compiler-grade linear-time dataflow bound.

Materializing interference cliques over live boundary sets is worst-case
`O((B + Q) * L^2)`. The returned diagnostic liveness itself can occupy
`O((B + Q) * L + I)` space because live sets are intentionally exposed at every
program point. Deterministic degree ordering and greedy coloring add ordinary
sorting/adjacency work over `L` locations and `I` edges.

These bounds are intentionally conservative. No optimal register-allocation,
minimum-spill, coalescing, or backend-wide complexity claim is imported.

## Frontier after integration

This implementation completes the Phase-48 roadmap hypothesis only after the
exact candidate and merged-main CI gates succeed. The next frontier must be chosen
by a fresh architecture audit. Likely backend directions include copy coalescing
or executable spill-code insertion/rewrite, but neither is pre-declared complete
or folded into this slice.
