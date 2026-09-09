# Phase 34 — dynamic forests and link-cut trees

Phase 34 adds a first-principles link-cut tree over a fixed set of dense vertex IDs. The represented graph is always a simple undirected forest. The data structure supports online `link`, exact-edge `cut`, connectivity queries, and the edge count on the unique represented-tree path.

## Represented forest versus auxiliary splay forest

The represented forest is the user-visible topology. It contains only edges accepted by `link` and loses only direct edges removed by `cut`.

The internal splay forest is different: it stores preferred paths created by `access`. A node's `left` and `right` fields are only auxiliary-splay children. Its `parent` field is deliberately dual-purpose: it is an auxiliary parent when the parent points back through `left` or `right`, otherwise it is a represented path-parent between auxiliary trees. `is_auxiliary_root` distinguishes those cases; treating every `parent` as an ordinary binary-tree parent would be incorrect.

`access(v)` repeatedly splays ancestors and replaces their preferred right child, leaving the old preferred child as a new auxiliary root with a represented path-parent. `make_root(v)` exposes the represented root-to-`v` path and lazily reverses it, changing the represented rooting without changing represented undirected edges.

## Splay and reversal invariants

Before a splay rotation, pending reversal flags on the auxiliary root-to-node path are pushed top-down. Rotations then preserve:

- child-to-parent consistency for every auxiliary `left`/`right` edge;
- the distinction between auxiliary parents and represented path-parents;
- `auxiliary_size = 1 + size(left) + size(right)`;
- represented connectivity and represented undirected edge membership.

A lazy reversal swaps the two auxiliary children and propagates a reversal bit. It does not change auxiliary subtree cardinality, so the stored size remains valid.

The executable diagnostic `valid_auxiliary_invariants()` checks child ranges, child/parent agreement, parent-pointer acyclicity, auxiliary-child acyclicity, complete auxiliary-forest coverage, and every stored auxiliary subtree size. It intentionally does not pretend that auxiliary edges are represented-tree edges.

## Operation proof obligations

### `link(u, v)`

`make_root(u)` first makes `u` the represented root. If `v` already has root `u`, adding the edge would create a cycle and is rejected. Otherwise setting `u`'s parent to `v` adds exactly one represented edge between two trees.

Self-links are rejected, so the represented topology remains a simple forest.

### `cut(u, v)`

After `make_root(u); access(v)`, the unique represented `u`-to-`v` path is one auxiliary splay tree ordered from `u` to `v`. The pair is a direct represented edge exactly when `u` is `v`'s left child and `u` has no right child on that exposed path. Only then is the parent/child connection removed. A disconnected pair or two non-adjacent connected vertices is rejected.

The operation is symmetric in its endpoint order because it first makes the first endpoint the represented root.

### Connectivity

Two vertices are connected exactly when `find_root` reaches the same represented root. The query mutates only preferred-path structure; it does not change represented topology.

### Path edge distance

After `make_root(u)` and `access(v)`, a connected `u`-to-`v` path is exactly the auxiliary tree rooted at `v`. Its stored `auxiliary_size` counts path vertices, so the exact number of represented edges is `auxiliary_size - 1`.

Disconnected path queries are rejected rather than returning a sentinel distance.

## Complexity boundary

Standard link-cut-tree analysis gives amortized `O(log V)` per `link`, `cut`, connectivity, and path query over an operation sequence, with `O(V)` resident storage.

This is explicitly an amortized bound, not a worst-case-per-operation claim. A single auxiliary splay tree may be tall, and one `push_path`/splay can therefore take `O(V)` in the worst case before amortization across the sequence.

## Verification

Focused verification uses repository-native test macros and strict warning flags:

- deterministic chain/tree cases for link, cut, repeated evert-style path exposure, and symmetric path distances;
- cycle-creating link rejection, self-link rejection, non-edge/non-adjacent cut rejection, disconnected path rejection, and vertex-bound validation;
- reverse-endpoint cuts to prove direct-edge semantics are orientation-independent;
- 30,000 fixed-seed mixed operations over 36 vertices against an independent naïve adjacency-matrix forest with BFS connectivity/path-distance queries;
- auxiliary structural invariants checked after every randomized operation;
- GCC release, Clang release, and GCC ASan+UBSan focused builds.

The naïve oracle never uses link-cut-tree operations, preferred paths, or splay state.

## Deliberate scope boundary

This phase does not implement path sums/min/max, node or edge weights, subtree aggregates, dynamic vertex creation, parallel represented edges, or a generic library wrapper. The purpose of the slice is the dynamic-forest topology machinery and its amortized contract.

## Sealed checkpoint

Phase 34 is sealed after the exact candidate tree reached `main` as `234984b29de44d546fba6c07598e9231e1e33a7b` and push CI run `34321406201` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. The independent 30,000-operation naïve-forest differential and executable auxiliary invariants remain the correctness evidence for this topology layer.

The next frontier is Phase 35 — augmented dynamic trees. It will preserve the sealed represented-forest topology contract while adding node values, point assignment, and exact represented-path sums whose auxiliary aggregates must remain correct across access, rotation, and lazy reversal. Weighted aggregates are intentionally a new phase because they add representability and aggregate-maintenance proof obligations rather than merely another topology query.
