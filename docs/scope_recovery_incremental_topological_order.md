# Scope recovery: incremental DAG topological ordering

## Coverage decision

A fresh post-Phase-69 live-code, pull-request-history, and branch audit after the
merged epsilon-NFA determinization checkpoint found static topological sorting,
SCCs, and DAG transitive reduction, but no maintained topological order under
online edge insertions. Existing static routines can recompute an order after
an update; they do not preserve mutable ordering state or provide transactional
cycle rejection.

This recovery slice therefore changes proof model again rather than extending the
recent automata work. It adds an online insertion-only DAG order with fixed
vertices and explicit order/position state. It does not resume the frozen
compiler/backend surface.

## Capability

`IncrementalTopologicalOrder(n)` starts with vertices `0..n-1`, no edges, and the
identity topological order. `try_add_edge(u, v)` validates endpoints and then:

- rejects self-loops transactionally;
- accepts parallel edges, matching the repository `Graph` multigraph semantics;
- accepts immediately when `u` already precedes `v`;
- otherwise searches forward from `v`, restricted to vertices whose current
  position is at most the current position of `u`;
- rejects without mutation if that search reaches `u`, because `u -> v` would
  close a directed cycle;
- otherwise stable-moves the reached set immediately after `u`, updates the
  inverse position map, and inserts the edge.

The public surface exposes vertex/edge counts, the maintained order, and the
inverse position of a vertex. It deliberately does not expose or duplicate a
second mutable `Graph` API.

## Ordering invariant

Before every successful insertion, `position[x] < position[y]` for every stored
edge `x -> y`, and `order` / `position` are inverse permutations.

For a backward proposed insertion `u -> v`, let `F` be exactly the vertices
reachable from `v` through stored edges while remaining at positions no greater
than `position[u]`.

- If `u` belongs to `F`, a pre-existing path `v ->* u` plus the proposed
  `u -> v` forms a directed cycle, so rejection is necessary. Because the graph
  and order have not yet been mutated, rejection is transactional.
- Otherwise, stable-moving `F` directly after `u` makes the proposed edge
  forward. Internal `F -> F` edge order is preserved.
- A stored edge from `F` to an outside vertex cannot target a vertex at or before
  `u`: that target would also be reachable inside the restricted search and
  would belong to `F`.
- A stored edge from outside into `F` cannot originate after `u`: before the
  update the maintained order was already topological, so such an edge would
  have pointed backward into a vertex at or before `u`.

Thus every previously stored edge remains forward after the stable relocation,
and the newly inserted edge is also forward.

## Complexity boundary

The implementation intentionally favors a small first-principles invariant over
an asymptotically optimal dynamic-topological-order data structure. A backward
insertion can scan `O(V+E)` reachable state and rebuild the `O(V)` permutation,
so the worst-case update cost is `O(V+E)` with `O(V+E)` resident graph/order
storage plus `O(V)` update workspace. Forward insertions are `O(1)` apart from
`Graph` adjacency growth.

No claim is made for edge deletion, dynamic vertex insertion, concurrent updates,
or the stronger bounds of specialized incremental topological-order algorithms.

## Verification

Deterministic regressions cover no-op-order forward insertion, stable reordering,
transactional cycle rejection, self-loop rejection, parallel edges, and endpoint
validation.

The fixed-seed randomized suite executes 600 traces with up to 13 vertices and
160 insertion attempts per trace. An independent oracle appends the proposed
edge to a plain edge list and runs Kahn's algorithm from scratch to decide
whether a cycle exists. After every operation the tests independently verify:

- the order is a permutation of every vertex;
- `position` is its exact inverse;
- every accepted edge points forward;
- the accepted-edge count matches production state;
- a rejected insertion leaves both edge count and order unchanged.

The Kahn recomputation is test-only; production never calls the sealed static
topological sorter or rebuilds indegrees to maintain its order.

## Scope boundary

This slice stops at insertion-only exact DAG ordering. Edge deletion, fully
dynamic ordering, batched updates, dynamic transitive closure, and optimal
incremental-order complexity are separate architectural hypotheses and are not
implied by this recovery checkpoint.
