# Scope recovery: online bridge and 2-edge connectivity

## Coverage decision

A fresh live audit after the insertion-only dynamic Aho checkpoint merged as
`main@609affcec45c6c46948adbceee82e035244d0f72` found zero open pull
requests and zero open issues. Default-branch code, branch names, commit search,
and implementation PR history contained no online / incremental bridge
connectivity structure.

The repository already contains static low-link-related decomposition surfaces,
rollback DSU, block-cut decomposition, cactus decomposition, and strong
orientation. This slice deliberately changes the state model: undirected
multigraph edges arrive online, and the structure maintains current connected
components, 2-edge-connected components, and the number of bridges without
rerunning static DFS after every insertion.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical ROADMAP are untouched.

## Production contract

`algorithms::graphs::OnlineBridgeConnectivity` owns a fixed undirected vertex
set and supports insertion-only multigraph edges.

- `add_edge(u,v)` accepts ordinary edges, parallel edges, and self-loops and
  returns a monotone insertion id;
- `bridge_count()` reports the exact number of current graph bridges;
- `component_count()` reports current connected components;
- `connected(u,v)` answers ordinary undirected connectivity;
- `same_two_edge_component(u,v)` reports whether no single edge deletion can
  disconnect the two vertices;
- invalid vertices reject before graph state changes;
- the edge id space is checked before mutation;
- `valid_invariants()` checks the internal two-layer DSU / bridge-forest
  representation.

Edge deletion, vertex insertion/removal, bridge enumeration by historical edge
id, articulation points, vertex-biconnectivity, dynamic MST, persistence,
rollback, concurrent mutation, and fully dynamic connectivity are outside this
slice.

## Representation invariant

The structure follows the first-principles online bridge method with two
disjoint-set layers.

`dsu_2ecc` identifies current 2-edge-connected components. `dsu_cc`
identifies current ordinary connected components after normalizing through the
2-edge-component representative. A parent forest connects current
2-edge-components by the edges that are still bridges.

For every connected component, contracting each 2-edge-connected component
therefore leaves a tree. Its edges are exactly the current bridges.

Constructor-sized scratch arrays hold path/LCA state, so normal edge updates do
not need per-operation scratch allocation.

## Insertion between different connected components

If the endpoint 2-edge-components belong to different connected components, the
new edge is the only connection between those components and is therefore a new
bridge.

Production reroots the smaller connected-component forest at its endpoint,
links that root beneath the other endpoint, unions the connected-component DSU,
and increments the bridge count. Union by connected-component size bounds how
often a vertex participates in rerooting over a growing execution.

## Insertion inside one connected component

If the endpoints are already in the same 2-edge-connected component, the new
edge changes no bridge relation.

Otherwise the new edge closes one cycle in the current bridge forest. Production
walks both endpoint-to-root paths in alternating order until their first repeated
2-edge-component representative identifies the LCA. Every bridge strictly
between either endpoint and that LCA lies on the new cycle and ceases to be a
bridge.

Those path representatives are contracted into the LCA in `dsu_2ecc`, and the
bridge count decreases by exactly the number of contracted bridge-forest edges.

This also handles parallel edges correctly: after the first edge connects two
components as a bridge, a second parallel edge closes a length-two multigraph
cycle and removes that bridge. Self-loops have both endpoints in the same
2-edge-component and change no bridge state.

## Complexity / non-claims

With `V` fixed vertices and `M` inserted edges, the classical two-DSU online
bridge structure gives amortized near-logarithmic update behavior from
path compression plus union-by-size rerooting. More concretely:

- connectivity / 2-edge-component queries follow DSU parent chains with bounded
  path compression on mutation paths;
- every successful union reroots the smaller connected component;
- every bridge-forest edge that is contracted by a cycle disappears from the
  bridge forest permanently;
- resident production storage is `O(V)`;
- `valid_invariants()` is an intentionally linear-to-near-linear structural
  diagnostic over the fixed vertex state.

This slice does **not** claim a worst-case logarithmic bound for every insertion,
fully dynamic deletions, bridge enumeration, lock-free behavior, or any
benchmark-backed speedup over rerunning low-link DFS.

## Independent verification

The committed oracle is structurally independent from the production DSU
method. Tests retain the full inserted undirected multigraph externally, assign
an edge id to every parallel/self-loop occurrence, and after updates:

1. rebuild ordinary adjacency with edge identities;
2. rerun Tarjan low-link DFS to classify bridges in the multigraph;
3. compute ordinary connected components;
4. remove oracle bridges and compute exact 2-edge-connected component labels;
5. compare bridge count, connected-component count, every vertex-pair
   connectivity query, and every vertex-pair 2-edge-connectivity query against
   production.

Deterministic coverage includes:

- isolated vertices and invalid-vertex rejection;
- a chain whose edges begin as bridges;
- closing a triangle and eliminating all chain bridges on that cycle;
- parallel-edge elimination of a bridge;
- self-loops;
- two cyclic blocks joined by bridges, followed by a large cycle that contracts
  both bridges.

The randomized corpus performs 120 fixed-seed multigraph traces over 1..16
vertices with 140 insertions each. Parallel edges and self-loops arise naturally,
and the independent Tarjan/component oracle is rebuilt after every insertion.

A supplementary model-level differential sanity pass was also performed while
reviewing the recurrence. It is not a compiler/sanitizer claim. Exact repository
GCC release, Clang release, and GCC ASan+UBSan CI on the pull-request head are
required before integration.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/graphs/online_bridge_connectivity.hpp`;
- `tests/test_online_bridge_connectivity_cases.hpp`;
- `docs/scope_recovery_online_bridge_connectivity.md`.

The isolated recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. No CMake, test-main, README, historical ROADMAP, workflow,
benchmark, frozen compiler/backend, or temporary-file change is required.

Base: `609affcec45c6c46948adbceee82e035244d0f72`.
