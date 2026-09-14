# Scope recovery: Suurballe two edge-disjoint shortest paths

## Coverage decision

A fresh live audit at `main@3a94d11ec010d521d51ff56b5ea38e8f7df6ef68`
found no Suurballe implementation in default-branch code, pull-request history,
or the branch namespace. The repository already has single-path shortest paths,
Yen k-shortest loopless paths, max flow/min cut, min-cost flow, and many later
graph capabilities, but none of those expose the direct two-path
potential/reversal/cancellation proof model recovered here.

This slice follows `docs/scope_recovery_after_phase69.md`. The historical
compiler/backend ROADMAP remains frozen prospectively and is not modified.

## Production contract

`suurballe_two_edge_disjoint_shortest_paths(graph, source, target)`:

- requires a directed graph and distinct valid endpoints;
- validates every stored edge globally and rejects any negative weight, including
  edges unreachable from the requested source;
- treats every adjacency entry as a distinct logical edge, so parallel and
  antiparallel copies retain exact `(from, adjacency_index)` identity;
- accepts self-loops but never returns one in a loopless path;
- returns `nullopt` when fewer than two edge-disjoint source-target paths exist;
- otherwise returns two loopless original-edge witnesses whose total cost is
  minimum among all edge-disjoint path pairs;
- rejects only when the selected exact public path/total cost is outside signed
  64-bit range, rather than because a larger non-optimal candidate overflowed.

## Suurballe proof obligation

The first Dijkstra pass gives shortest distances `d`. Every non-path edge
`u -> v` receives reduced cost `w(u,v) + d(u) - d(v) >= 0`. Every exact edge copy
of the first shortest path is removed in its forward direction and replaced by a
zero-cost reverse arc.

A shortest path in this residual graph therefore chooses the cheapest admissible
deviation while reverse arcs represent cancellation of first-path edges. XORing
the first path with the second residual path produces an integral value-two
source-target edge flow of minimum original total cost. Cancelling opposite uses
and decomposing that flow yields two original-edge-disjoint source-target paths.

The implementation uses repository `BinaryHeap` Dijkstra passes. Distances use a
private two-limb unsigned accumulator, so representability is checked only when
narrowing the selected public result. The first-principles implementation does not
hide the subject algorithm behind min-cost flow.

## Verification

Focused pre-upload verification on the live `Graph`, `BinaryHeap`, and repository
test framework passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic evidence includes domain validation, globally unreachable negative
edge rejection, parallel-edge identity, zero-weight self-loops, no-pair behavior,
exact `INT64_MAX` total cost, total-cost overflow, and a huge non-optimal path
beside a representable optimum.

A dedicated reversal regression uses edges `s->a`, `s->b`, `a->b`, `a->t`,
`b->t` with insertion order making `s-a-b-t` the first shortest path. Simply
deleting that path destroys the optimum pair; production must reverse/cancel
`a->b` and recover `s-a-t` plus `s-b-t`.

The primary randomized oracle is structurally independent of Suurballe. For 700
fixed-seed directed multigraphs with 2..7 vertices and up to 13 edge copies, it
enumerates every simple source-target path by exact adjacency-edge identity,
enumerates every edge-disjoint path pair, and selects the exact minimum total.
Production existence/cost must match that optimum; every returned edge witness,
loopless vertex sequence, edge-disjointness relation, and exact cost is replayed.
An additional local 5,000-graph optimized stress corpus also passed and is not
added to repository CI.

## Complexity and non-claims

The direct implementation performs two heap-Dijkstra passes plus linear residual
construction/decomposition. With stale heap entries it conservatively claims
`O(V + E + E log(E+1))` time and `O(V+E)` auxiliary/result scale, excluding the
returned path vectors.

No negative-edge Suurballe variant, vertex-disjoint transformation, more-than-two
path enumeration, Eppstein bound, min-cost-flow equivalence API, canonical global
tie order among all optimal pairs, or benchmark speedup is claimed.
