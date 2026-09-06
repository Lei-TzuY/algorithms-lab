# Milestone 1 invariants and correctness notes

This document records the proof obligations that matter most for the first vertical slice. It is intentionally concise: implementations and tests remain the executable source of truth.

## Binary search

**Precondition:** the input range is sorted under the supplied comparison.

The search maintains a half-open interval `[low, high)`. If an element equivalent to the key exists, its first occurrence is never discarded from that interval. Every element before `low` is strictly smaller than the key. On termination `low == high`, so `low` is exactly the lower-bound position; equality at that position decides success.

Time: `O(log n)`. Extra space: `O(1)`.

## Merge sort

Each recursive call returns with its subrange sorted. During merge, the emitted prefix is the sorted merge of the consumed prefixes of the left and right halves. Selecting from the left on equality preserves stability.

Time: `O(n log n)`. Extra space: `O(n)`.

## Quicksort

Milestone 1 uses a **median-of-three** pivot candidate (first, middle, last) and a Dutch-national-flag three-way partition. During partition the range is split into `< pivot`, `== pivot`, unknown, and `> pivot` regions. At completion, only the two strict regions require recursion. Recursing into the smaller side first bounds stack depth to `O(log n)` even when comparisons degrade to worst-case `O(n^2)`.

Average time: `O(n log n)`. Worst-case time: `O(n^2)`. Auxiliary stack: `O(log n)` with this control structure.

## Binary heap

For every non-root node, its parent has at least as much priority under the heap comparator. `push` can violate this only along the inserted node's ancestor chain; `sift_up` repairs that chain. `pop` replaces the root with the last element, so only one downward path can violate the property; `sift_down` repairs it.

`push`/`pop`: `O(log n)`. `top`: `O(1)`. Storage: `O(n)`.

## Disjoint-set union

Every element follows parent pointers to exactly one representative root `r` satisfying `parent[r] == r`. Union-by-size only attaches one root to another root. Path compression changes intermediate parent pointers directly to the same representative and therefore preserves set membership. Component sizes are stored only at roots.

Amortized `find`/`unite`: `O(alpha(n))`. Storage: `O(n)`.

## Breadth-first search

When vertex `u` leaves the queue, `distance[u]` is the minimum number of edges from the start vertex. A vertex is discovered once, from an already-minimal layer, and receives distance `distance[u] + 1`. Queue order therefore processes nondecreasing layers. Parent pointers form a shortest-path tree for reachable vertices.

Time: `O(V + E)`. Storage: `O(V)`.

## Depth-first search / components / cycle detection

DFS marks each visited vertex once. Connected components on an undirected graph start a new DFS exactly when a vertex has not been visited by any prior component. Directed cycle detection uses white/gray/black states: an edge to a gray vertex is precisely a back edge to the active DFS path. Undirected cycle detection excludes the single parent edge while treating self-loops and parallel parent edges as cycles.

Time: `O(V + E)`. Storage: `O(V)` plus recursion stack for cycle detection.

## Topological sorting

Kahn's algorithm keeps an indegree count equal to the number of incoming edges not yet removed. A zero-indegree vertex can safely be emitted because no remaining prerequisite points to it. If fewer than `V` vertices are emitted, the remaining subgraph has no zero-indegree vertex and therefore contains a directed cycle.

Time: `O(V + E)`. Storage: `O(V)`.

## Dijkstra

**Precondition:** every edge weight is non-negative; the implementation validates the entire graph and rejects any negative edge.

The heap may contain stale candidates. When a non-stale minimum-distance entry `u` is removed, every alternative path to `u` through an unsettled vertex is no shorter because all edge weights are non-negative. Thus `distance[u]` is final. Relaxation preserves the best discovered distance for every vertex.

Time with binary heap: `O((V + E) log V)` (duplicate stale entries allowed). Storage: `O(V + E)` including the graph.

## Bellman-Ford

After pass `k`, every shortest path using at most `k` edges has been accounted for by repeated edge relaxation. Any simple shortest path without a reachable negative cycle uses at most `V-1` edges, so `V-1` passes suffice. A further relaxable reachable edge proves a reachable negative cycle.

Time: `O(VE)`. Extra algorithm storage: `O(V)`.

## Differential validation

Randomized tests use fixed seeds. Sorting is checked against `std::sort` as an oracle (never as the implementation under test). Heap behavior is checked against `std::multiset`. On hundreds of generated non-negative weighted graphs, Dijkstra distances are compared vertex-for-vertex with the independently implemented Bellman-Ford result, including unreachable vertices, self-loops, parallel edges, directed and undirected graphs, and zero-weight edges.
