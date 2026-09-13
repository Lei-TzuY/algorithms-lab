# Scope recovery: bounded exact DAG linear-extension counting

## Coverage decision

A fresh live audit after bounded exact binary permanent reached merged `main`
found no linear-extension / topological-order counting production API in default-
branch code, pull-request history, or the branch namespace. The repository has
static and incremental topological ordering, but those surfaces produce or
maintain one valid order rather than count every order consistent with a partial
order.

This slice deliberately changes proof model again. Binary permanent used Ryser
inclusion-exclusion; minimum dominating set used exact branch-and-bound; Tutte
used deletion/contraction. Linear-extension counting instead uses dynamic
programming over order ideals of a DAG.

The Phase-45--69 compiler/backend surface remains frozen. Historical
`ROADMAP.md` presentation is untouched; `docs/scope_recovery_after_phase69.md`
plus fresh live coverage remain prospective authority.

## Production contract

`count_dag_linear_extensions(graph)` accepts the repository `Graph` abstraction
with these semantics:

- input must be directed and acyclic; undirected or cyclic input rejects;
- `V <= 20` is an explicit exact bound;
- parallel arcs collapse to one precedence relation;
- stored edge weights are intentionally ignored;
- the empty DAG has exactly one linear extension;
- the result returns the exact count, vertex count, number of reached order-ideal
  states, and the lexicographically smallest topological order;
- execution and the returned witness are deterministic.

Self-loops are cycles and therefore reject rather than silently producing a zero
count. A DAG always has at least one topological order.

## Order-ideal DP obligation

For every vertex `v`, production materializes a predecessor bitmask `pred[v]`.
Parallel arcs naturally deduplicate through that bitmask.

For a subset `S`, `dp[S]` is the number of valid topological prefixes whose set
of already placed vertices is exactly `S`. The base state is `dp[empty] = 1`.
A vertex `v` may extend state `S` exactly when `v` is not in `S` and every
predecessor of `v` lies in `S`. The transition

`dp[S union {v}] += dp[S]`

is therefore a bijection between valid prefixes ending at `S union {v}` and a
valid shorter prefix together with its final appended vertex. Induction on
`|S|` gives the exact linear-extension count at the full set.

Before the DP, a smallest-id eligible-vertex scan constructs the
lexicographically smallest topological witness. Failure to find any eligible
vertex before all vertices are placed proves a directed cycle and rejects the
input.

## Exact arithmetic boundary

Every DAG on `n` labeled vertices has at most `n!` linear extensions. Under the
public bound,

`20! = 2,432,902,008,176,640,000 < 2^64 - 1`.

Thus every supported mathematical answer is exactly representable in
`uint64_t`. Production still checks every DP addition and fails closed on an
unexpected overflow rather than relying only on the theorem-level bound.

The full state space has at most `2^20 = 1,048,576` subsets.

## Independent verification

Focused candidate execution passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic evidence includes empty and edgeless DAGs, a chain, a diamond, two
independent chains, parallel copies with differing signed weights, self-loop and
directed-cycle rejection, undirected rejection, exact 20-vertex acceptance, and
21-vertex rejection. The 20-vertex edgeless case exercises the exact upper count
`20!` and all `2^20` reachable ideal states.

The primary randomized oracle is structurally independent from the subset DP:
it enumerates every vertex permutation, checks every canonical precedence pair,
and directly counts valid orders. Across 700 fixed-seed graphs with `0..8`
vertices, half are arbitrary directed multigraphs (so the oracle independently
detects cycles through a zero valid-permutation count) and half are generated
DAG multigraphs. Production must exactly match both the count and the first
lexicographic valid permutation, and replay deterministically.

The permutation oracle contains no order-ideal recurrence or Kahn/MCS-style
counting logic.

## Complexity / non-claims

Predecessor materialization costs `O(E)`. The deterministic topological witness
uses `O(V^2)` direct scans. The exact subset DP costs `O(V * 2^V)` time and
`O(2^V + V)` storage under `V <= 20`.

This is not a polynomial-time linear-extension counter, approximate sampler,
Markov-chain estimator, arbitrary-precision counter, unlabeled-poset
isomorphism/canonicalization API, dynamic DAG counter, or large-instance
practicality claim.
