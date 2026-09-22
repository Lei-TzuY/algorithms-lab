# Scope recovery: exact minimum linear arrangement

## Coverage decision

After deterministic FKM de Bruijn sequences merged as
`main@221113780601f0d376bcc5532bd1ebf7da23b9cb`, a fresh live audit found
zero open pull requests and zero open issues.

Default-branch code, recovery branches, and pull-request history contained no
minimum-linear-arrangement / MinLA implementation or occupied branch.

The repository already contains many graph capabilities, including min-cut,
treewidth/pathwidth-adjacent decomposition work, dynamic connectivity, shortest
paths, and ordering-related structures. None of those exposes the MinLA
objective

`sum_{ {u,v} in E } |position(u)-position(v)|`

or the prefix-cut subset dynamic program used here.

This slice is therefore a new exact graph-layout capability rather than an
alternate implementation of an existing graph primitive.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; frozen compiler/backend history is not
resumed.

## Public contract

`minimum_linear_arrangement(vertex_count, edges)` accepts a simple undirected
graph on vertices `0..vertex_count-1`.

The result contains:

- the exact minimum arrangement cost;
- one permutation of every vertex;
- specifically, the lexicographically smallest permutation among all
  minimum-cost arrangements.

Input validation rejects:

- more than 22 vertices;
- an endpoint outside the vertex range;
- self-loops;
- duplicate undirected edges, including reversed duplicates.

The empty graph on zero vertices is valid and returns cost zero with an empty
ordering.

The 22-vertex limit is an explicit exact-algorithm boundary. It prevents the
`2^n` state space from being disguised as a scalable polynomial algorithm.

## Prefix-cut identity

Fix an ordering `v_1,...,v_n`.

For each `i=1,...,n-1`, let

`P_i={v_1,...,v_i}`

and let `cut(P_i)` be the number of graph edges with exactly one endpoint in
`P_i`.

Consider one edge whose endpoints occur at positions `a<b`.

That edge crosses exactly the prefix cuts

`P_a, P_{a+1}, ..., P_{b-1}`.

It is therefore counted exactly `b-a` times, which is exactly the edge's
linear-arrangement contribution.

Summing first over prefix cuts or first over graph edges gives the identity

`MinLA(ordering) = sum_{i=1}^{n-1} cut(P_i)`.

This identity is the production DP proof boundary.

## Exact subset dynamic program

Production stores a state by the set `R` of vertices that remain unplaced.

The already placed prefix is `V\R`.

If vertex `v in R` is chosen next, the new remaining set is

`R' = R\{v}`.

The newly completed prefix is `V\R'`. For an undirected graph,

`cut(V\R') = cut(R')`.

Therefore the exact recurrence is

`dp[R] = min_{v in R} (cut(R\{v}) + dp[R\{v}])`

with

`dp[empty]=0`.

Starting from `R=V`, the recurrence accumulates every non-final prefix cut
exactly once; the final full-vertex prefix has cut zero.

## Lexicographically smallest optimum

At each state, candidate next vertices are examined in increasing vertex id.

Production replaces the current choice only on a strictly smaller cost, not on
an equal cost.

Inductively, `dp[R\{v}]` already reconstructs the lexicographically smallest
optimal suffix for that remaining set. Therefore choosing the smallest first
vertex among equal-cost transitions yields the lexicographically smallest
optimal ordering for `R`, and in particular for the full vertex set.

This tie behavior is part of the public contract and is independently checked
against lexicographically enumerated permutations.

## Cut-table recurrence

For every subset mask, production precomputes its edge cut count.

Let `S = T union {v}`, where `v notin T`.

Starting from `cut(T)`:

- each edge from `v` to a vertex outside `S` becomes a new crossing edge;
- each edge from `v` to a vertex inside `T` was crossing before `v` was
  added and becomes internal afterward.

Thus

`cut(S) = cut(T) + degree(v) - 2 * neighbors(v,T)`.

Adjacency is stored in one 32-bit word because the public exact boundary is at
22 vertices. Population count supplies the inside-neighbor count directly.

## Independent verification

The committed oracle does **not** use prefix cuts, subset states, or the
production recurrence.

For a candidate permutation it explicitly builds the vertex-position array and
computes

`sum |position(u)-position(v)|`

over the original edge list.

It then enumerates permutations lexicographically with
`std::next_permutation`. Updating the oracle only on a strictly lower cost
makes the first optimum exactly the lexicographically smallest optimum.

Committed coverage includes:

- empty, singleton, and edgeless graphs;
- a path with a known optimum;
- `K_4`, whose arrangement cost is independent of permutation;
- vertex-limit rejection;
- out-of-range endpoints;
- self-loops;
- duplicate and reversed-duplicate edges;
- **every simple graph on 0 through 5 vertices**;
- 140 fixed-seed random graphs on 0 through 7 vertices, compared against full
  permutation enumeration;
- a medium 14-vertex graph whose returned ordering is replayed through the
  direct objective calculator.

The exhaustive 0..5-vertex sweep covers 1,100 distinct simple graphs.

Supplementary model-level verification independently replayed the same
production recurrence against direct permutation enumeration for all 1,100
small graphs plus 500 additional random graphs through seven vertices and found
zero cost or tie-break mismatch. This is supporting evidence only; repository
CI remains the integration gate.

## Complexity boundary

Let `n <= 22` and `m=|E|`.

Input validation and adjacency construction are `O(n+m)`.

There are `2^n` subsets.

- cut-table construction: `O(2^n)` fixed-word operations;
- subset DP: `O(n 2^n)`;
- reconstruction: `O(n)`;
- storage: `O(2^n + n)`.

The direct permutation oracle is factorial and is intentionally restricted to
tiny tests.

## Non-claims

This slice does not claim:

- polynomial-time MinLA;
- approximation guarantees for larger graphs;
- weighted edges;
- directed arrangements;
- circular arrangements;
- cutwidth or bandwidth optimization;
- branch-and-bound scalability beyond the explicit 22-vertex DP boundary;
- benchmark-backed performance superiority.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/graphs/minimum_linear_arrangement.hpp`;
- `tests/test_minimum_linear_arrangement_cases.hpp`;
- `docs/scope_recovery_minimum_linear_arrangement.md`.

Pre-PR hardening includes:

- avoiding braced initializer commas directly inside assertion macros;
- explicitly including `<cstdint>` in tests rather than relying on transitive
  includes.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `221113780601f0d376bcc5532bd1ebf7da23b9cb`.
