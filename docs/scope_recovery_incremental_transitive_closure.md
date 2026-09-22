# Scope recovery: incremental directed transitive closure

## Coverage decision

A fresh post-min-max-heap live audit at
`main@1b73a27229b22fc3963b365ca62803b7b51677a3` found zero open pull
requests and zero open issues. Default-branch code, branch names, and pull-request
history contained no incremental/dynamic transitive-closure implementation.

The repository does already contain `IncrementalTopologicalOrder`, but that
surface deliberately maintains one valid DAG ordering and transactionally
rejects cycle-creating insertions. This recovery targets a different state model:
cycles are allowed, every insertion is retained, and the structure maintains the
complete directed reachability relation plus strongly-connected reachability.

Prospective authority remains `docs/scope_recovery_after_phase69.md`; the
historical compiler/backend roadmap remains frozen.

## Production contract

`algorithms::graphs::IncrementalTransitiveClosure` owns a fixed vertex set and
supports insertion-only directed edges.

- reachability is reflexive: every vertex reaches itself by a zero-edge path;
- `add_edge(u,v)` records one unique explicit directed edge and returns false
  only for an exact duplicate edge;
- adding an edge already implied transitively still records a new explicit edge;
- cycles and self-loops are accepted;
- `reachable(u,v)` answers exact current reachability;
- `same_strong_component(u,v)` reports mutual reachability;
- `would_create_cycle(u,v)` reports whether a new `u -> v` relation closes a
  directed cycle containing that edge;
- `reachable_count_from(u)` counts the reflexive reachable set;
- invalid vertices reject with `std::out_of_range`;
- `valid_invariants()` independently recomputes closure from explicit edges.

Deletion, vertex insertion/removal, shortest paths, path reconstruction, edge
multiplicity beyond unique explicit pairs, and concurrent mutation are outside
this slice.

## Incremental closure invariant

Assume the stored closure is exact before inserting one previously absent
explicit edge

`u -> v`.

If `v` is already reachable from `u`, the transitive closure does not change.

Otherwise every newly created path must have the form

`x => u -> v => y`

where `x => u` and `v => y` were both already reachable before the
insertion. Conversely every such predecessor/successor pair becomes reachable
after insertion.

Production therefore snapshots:

- every old predecessor of `u`;
- the complete old successor bitset of `v`;

then ORs that successor bitset into each predecessor row exactly once.

Because the pre-insertion relation is already transitive, this single
predecessor × successor product is complete even when the new edge creates a
cycle. No fixed-point iteration is required for one insertion.

## Transactional allocation boundary

When closure bits must change, production first copies the successor bitset and
materializes the predecessor list. Those are the only allocating steps in the
closure update. The explicit-edge bit and edge count are mutated only after both
allocations succeed.

Therefore allocation failure cannot leave an explicit edge recorded without its
corresponding closure update. If the new explicit edge is already transitively
implied, no closure-side allocation is required.

## Representation

Both explicit edges and reachability are dense row-major bitsets implemented as
`vector<vector<uint64_t>>`. Bit `j` in row `i` represents relation
`i -> j`.

The dense representation is deliberate for exact all-pairs reachability:

- one reachability query is one bit test;
- SCC membership is two bit tests;
- an insertion scans predecessor rows and ORs complete successor words;
- 64-vertex word boundaries are part of the tested contract.

## Complexity / non-claims

Let `V` be the fixed vertex count, `W = ceil(V/64)`, and `P <= V` the
number of old predecessors of the inserted edge source.

- `reachable`, `explicit_edge`, `same_strong_component`, and
  `would_create_cycle`: `O(1)`;
- `reachable_count_from`: `O(W)`;
- one closure-changing insertion: `O(V + P*W)`, hence
  `O(V + V^2/64)` in the dense worst case;
- an already implied unique explicit edge: `O(1)` after validation;
- resident storage: `O(V^2)` bits for reachability plus `O(V^2)` bits for
  explicit edges;
- `valid_invariants()`: diagnostic `O(V^3/64)` bitset Floyd-Warshall work.

These are direct structural bounds, not controlled benchmark claims. This slice
does not claim sparse-graph optimality, fully dynamic transitive closure,
subquadratic worst-case update bounds, cache optimality, or universal superiority
over repeated DFS/BFS.

## Independent verification

Before repository upload, a focused implementation of the exact production
recurrence passed:

- GCC C++20 with the repository strict warnings-as-errors profile;
- Clang C++20 with the same warning profile;
- GCC ASan + UBSan with fail-fast behavior.

That focused corpus used 400 fixed-seed random graphs with up to 23 vertices and
160 insertions per graph; sampled all-pairs reachability was checked against
ordinary queue-based BFS, while the production-style invariant replay used an
independent Floyd-Warshall recurrence.

Committed repo-native evidence adds:

- empty-graph and invalid-vertex behavior;
- reflexive reachability;
- duplicate explicit-edge semantics;
- transitively implied explicit edges;
- cycle creation and SCC membership;
- self-loop handling;
- a 130-vertex chain crossing multiple 64-bit storage words;
- fixed-seed randomized edge insertion checked repeatedly against direct BFS;
- independent full-state invariant reconstruction from the explicit-edge matrix.

The BFS oracle does not use the production predecessor × successor update. The
diagnostic Floyd-Warshall recurrence likewise recomputes closure from explicit
edges rather than replaying incremental updates.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/graphs/incremental_transitive_closure.hpp`;
- `tests/test_incremental_transitive_closure_cases.hpp`;
- `docs/scope_recovery_incremental_transitive_closure.md`.

The isolated recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. No CMake, test-main, README, historical ROADMAP, workflow,
benchmark, frozen compiler/backend, or temporary-file change is required.

Base: `1b73a27229b22fc3963b365ca62803b7b51677a3`.
