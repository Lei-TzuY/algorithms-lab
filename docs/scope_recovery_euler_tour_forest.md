# Scope recovery: Euler-tour-tree dynamic forest

## Coverage decision

Fresh post-Phase-69 recovery state after the metric-TSP double-tree checkpoint reached merged-main green was `main@400fc08845bbab7497f2bf06188227ed1492eac8`. Live code, PR-history, and branch searches found the sealed Link-Cut Forest family but no Euler-tour-tree implementation or `euler-tour` recovery branch. The already occupied `scope-recovery-fractional-cascading`, `scope-recovery-minimum-cycle-basis`, and `scope-recovery-general-graph-isomorphism` branches are intentionally untouched.

This is not a second wrapper around Link-Cut Tree. The sealed link-cut structure represents preferred paths and is path-query oriented. This slice represents each whole tree as one cyclic Euler sequence and maintains that sequence with first-principles implicit-treap split/merge operations. Its public focus is component topology: online link/cut, connectivity, and exact component cardinality.

## Production contract

`EulerTourForest` owns a fixed dense vertex set and supports:

- `link(u,v)` only when `u != v` and the vertices are in different represented trees;
- exact represented-edge `cut(u,v)` in either endpoint order;
- `connected(u,v)`;
- `component_size(v)` counting represented vertices, not Euler tokens;
- `edge_count()`;
- `valid_structure()` as an executable structural diagnostic.

Cycle-creating links, self-links, missing-edge cuts, self-edge cuts, and invalid vertex IDs are rejected. The represented graph is therefore always a simple forest. Public connectivity/cardinality semantics are independent of the treap seed.

## Euler-tour invariant

Every represented component owns one cyclic implicit sequence containing:

- exactly one **vertex token** for every represented vertex; and
- exactly two directed **edge tokens** `(u,v)` / `(v,u)` for every represented edge.

The implicit treap stores sequence size and vertex-token count. Parent pointers make the treap root observable from any token.

To link distinct components at `u,v`, production cyclically rotates each component so its vertex token is first, then concatenates

`T_u, (u,v), T_v, (v,u)`.

This is exactly an Euler traversal of the new tree. Edge records are canonicalized by `(min(u,v), max(u,v))`; the requested direction only chooses which canonical directed token participates first. A deterministic reverse-endpoint regression preserves a bug found in the first prototype, where canonical edge bookkeeping and call-order token orientation could disagree.

To cut edge `{u,v}`, production rotates the cyclic sequence to one directed edge token, removes that token and its reverse occurrence, and splits the remaining sequence at the reverse token. The two non-empty sequence fragments are precisely Euler tours of the two resulting components.

Therefore two vertices are connected exactly when their vertex tokens have the same implicit-treap root. Component size is exactly the root aggregate counting vertex tokens.

## Implicit-treap invariant and complexity boundary

Each token maintains:

1. implicit subtree size;
2. represented-vertex token count;
3. child-to-parent pointer consistency; and
4. max-heap order over `(mt19937_64 priority, unique serial)`.

Split and merge preserve inorder sequence order and recompute both aggregates. `valid_structure()` independently replays parent/heap/aggregate coverage, verifies both directed tokens for every edge, and checks the forest identity `components + edges = vertices`.

With `N = V + 2E = O(V)` in a forest, under the standard random-priority treap model, link, cut, connectivity, and component-size queries have expected `O(log V)` time; the ordered edge table contributes another `O(log V)` factor-free lookup term of the same asymptotic order. Resident storage is `O(V)`.

These are **expected** bounds under random priorities. This implementation does not claim deterministic or per-operation worst-case logarithmic time. A bad priority shape can make an operation and the recursive split/merge stack `O(V)`. `valid_structure()` is a diagnostic and is not included in operation bounds; its ordered pointer bookkeeping is `O(V log V)`.

## Verification

Focused repo-native execution on the final candidate passed:

- GCC C++20 with repository strict warnings-as-errors;
- Clang C++20 with repository strict warnings-as-errors; and
- actual GCC ASan+UBSan with leak detection.

Deterministic evidence covers reverse-endpoint link/cut orientation, component-size changes, cut/relink behavior, cycle rejection, self-link/self-cut rejection, missing-edge rejection, empty input, and vertex bounds.

The primary differential oracle is structurally independent: a `std::set` adjacency forest is rebuilt with ordinary BFS for connectivity and component cardinality. The committed randomized corpus performs 20,000 fixed-seed mixed operations over 40 vertices, checks exact edge count and the full ETT structural diagnostic after every step, and periodically compares every vertex pair plus every component size. A 50,000-operation focused stress run passed before upload.

The standard library is used only by the test oracle and by ordinary ownership/index containers, not as an Euler-tour-tree implementation.

## Non-claims

No represented-path aggregate, subtree aggregate under a chosen root, dynamic minimum spanning forest, fully dynamic non-forest graph connectivity, deterministic balanced-sequence bound, persistence, library dynamic-tree dependency, or replacement of the sealed Link-Cut Forest is claimed.
