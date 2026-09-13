# Scope recovery: exact A* under a validated consistent heuristic

## Coverage decision

A fresh post-Phase-69 coverage audit after the Sprague-Grundy impartial-game
checkpoint found no A* / best-first heuristic shortest-path production surface
in default-branch code, pull-request history, or branch namespace. The repository
already has exact Dijkstra/Bellman-Ford single-source paths and Johnson APSP, so
this slice does not add another unconstrained shortest-path variant. It adds a
different proof boundary: caller-supplied heuristic information is accepted only
after its consistency contract is replayed on the complete graph.

The Phase-45--69 compiler/backend excursion remains prospectively frozen under
`docs/scope_recovery_after_phase69.md`.

## Production contract

`a_star_shortest_path(graph, source, target, heuristic)`:

- accepts directed or undirected `Graph` instances;
- validates source/target and requires one heuristic value per vertex;
- requires globally non-negative edge weights and non-negative heuristic values;
- requires `h(target)=0`;
- validates `h(u) <= w(u,v)+h(v)` on every stored arc before search;
- returns `std::nullopt` distance plus empty path witnesses when target is
  unreachable;
- returns an exact `Weight` shortest distance when representable;
- returns both the vertex path and exact original adjacency-edge witnesses, so
  parallel edges are replayable rather than ambiguous;
- exposes deterministic settled/expansion order for the fixed graph and
  heuristic.

The implementation reuses the repository's first-principles `BinaryHeap`.

## Correctness obligation

For every path from vertex `v` to the target, repeated consistency plus
`h(target)=0` gives `h(v) <= path_cost(v,target)`, so the validated heuristic is
admissible on every vertex that can reach the target.

The queue key is `f(v)=g(v)+h(v)`. Consistency makes reduced edge costs
`w(u,v)+h(v)-h(u)` non-negative. Therefore the search is Dijkstra's settled-key
argument on those reduced costs: when a non-stale vertex is expanded, its `g`
value is its exact shortest distance. The first expansion of the target is thus
optimal and closed vertices never require reopening.

Duplicate queue entries are permitted because the educational `BinaryHeap` has
no decrease-key; stale entries are discarded by comparing their stored `g`
against the current best value.

### Wide arithmetic boundary

All public edge/heuristic values are non-negative signed-64 values, but a
non-optimal candidate path can exceed `INT64_MAX` while a different optimum is
representable. Production therefore stores `g` and `f` in a private two-limb
unsigned distance and narrows only the final target distance.

A settled shortest path can be chosen simple and therefore uses at most `V-1`
edges. With `V` represented by `size_t` and each edge at most `INT64_MAX`, the
path sum is below `2^127` on a 64-bit `size_t`; adding one signed-64 heuristic
still fits the two-limb unsigned domain. The helper nevertheless fails closed if
its high limb would wrap. A finite optimum above public `Weight` raises
`std::overflow_error`; an overflowing non-optimal candidate does not.

## Independent verification

Focused repo-native verification passes:

- strict GCC C++20 warnings-as-errors;
- strict Clang C++20 warnings-as-errors;
- actual GCC ASan+UBSan.

Deterministic evidence covers informative-vs-zero heuristic expansion, exact
parallel-edge witness replay, zero-weight self-loops, unreachable and
source-equals-target cases, malformed/negative/non-zero-goal/inconsistent
heuristics, globally unreachable negative-edge rejection, out-of-range vertices,
a huge non-optimal path beside a representable optimum, and an unrepresentable
finite optimum.

The primary randomized oracle is Floyd-Warshall, not another heap search:

- 300 fixed-seed arbitrary directed non-negative multigraphs with zero heuristic
  compare exact reachable/unreachable distances and replay returned edge
  witnesses;
- 300 fixed-seed forward DAG multigraphs first compute exact target distances by
  Floyd-Warshall and use those values as a maximally informative consistent
  heuristic; every source must still match the independent oracle exactly.

Random tests also require expansion-order uniqueness and target-last semantics
for successful searches.

## Complexity / non-claims

Global contract validation is `O(V+E)`. The stale-entry heap may contain `O(E)`
entries, so the direct code-local bound is
`O(V+E + E log(E+1))` time and `O(V+E)` auxiliary/result scale beyond the input
graph.

No unchecked admissible/inconsistent heuristic mode, reopen-on-inconsistency
variant, bidirectional A*, ALT/landmark preprocessing, dynamic graph support,
negative-edge support, benchmark speedup, or claim that A* expands fewer
vertices than Dijkstra on every valid heuristic is implied.
