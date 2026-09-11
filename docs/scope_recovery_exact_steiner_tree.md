# Scope recovery: exact terminal-subset Steiner tree

## Coverage decision

After exact two-phase simplex linear programming reached merged-main green, a
fresh live code and pull-request-history audit deliberately changed proof model
again. The repository contains shortest paths, spanning trees, flows, cuts,
matching, exact cover, matroid intersection, and many other graph capabilities,
but no Steiner-tree / Dreyfus-Wagner implementation or prior recovery PR.

This slice therefore adds exact fixed-parameter terminal-subset optimization
rather than farming another recent cycle, linear-programming, heap, string-index,
or frozen compiler/backend variant. The prospective post-Phase-69 recovery
boundary remains unchanged; stale historical ROADMAP presentation is not used to
resume the frozen backend sequence.

## Public contract

`minimum_steiner_tree(graph, terminals)` accepts an undirected `Graph` with
non-negative signed-64 edge weights and valid terminal vertices.

- duplicate terminals are canonicalized into ascending unique order;
- empty and singleton terminal sets have exact zero cost and no edges;
- self-loops and parallel edge copies follow the repository multigraph model;
- disconnected terminal sets return `std::nullopt`;
- a successful result contains exact total weight plus replayable original
  adjacency-edge witnesses;
- if the exact optimum is connected but exceeds public signed-64 `Weight`, the
  solver throws `std::overflow_error` rather than wrapping;
- a merely non-optimal overflowing candidate does not force failure when a
  representable optimum exists.

Negative edges are rejected globally. The classical non-negative Dreyfus-Wagner
closure used here is not silently generalized to negative-weight inputs.

## Dreyfus-Wagner invariant

For a canonical terminal subset `S` and graph vertex `v`, `dp[S][v]` is the
minimum exact cost of a connected subgraph that contains every terminal in `S`
and contains `v`.

Singleton states seed their terminal at zero cost. For a composite subset, the
merge recurrence

```
dp[S][v] = min(dp[A][v] + dp[S\A][v])
```

over non-empty proper bipartitions joins two optimal terminal subtrees at the
same vertex. After all subset merges for `S`, one multi-source Dijkstra closure
propagates that connected structure through non-negative graph edges. Every
proper submask is numerically smaller than `S`, so its closure is complete before
`S` is processed.

Production stores explicit reconstruction parents for seed, subset-merge, and
path-relaxation states. Dijkstra writes path parents only on strict improvement,
which also prevents zero-weight equal-cost cycles in the reconstruction graph.
The selected union is finally reduced to an acyclic forest by stable logical-edge
id while preserving terminal connectivity. Production checks that the cleaned
witness cost exactly equals the DP optimum; any disagreement fails closed as an
internal logic error.

This is the classical Dreyfus-Wagner optimality obligation. Randomized and
exhaustive tests are implementation evidence, not a proof of the recurrence.

## Exact arithmetic / representation boundary

Internally, costs use an extended unsigned state with separate meanings for
unreachable and "connected but larger than `INT64_MAX`". Saturating addition
therefore keeps an overflowing candidate available for connectivity propagation
without allowing it to wrap or eclipse a representable candidate. Only after the
full terminal subset is minimized does an unrepresentable optimum become the
public `std::overflow_error`.

The terminal subset mask uses `size_t`; terminal counts or state products that
cannot be represented are rejected with `std::length_error`. This baseline does
not claim practical scalability to large terminal sets.

## Verification

Focused pre-upload verification passes repository-equivalent strict warning
flags under GCC and Clang plus an actual GCC ASan+UBSan build: 4/4 native test
groups in each configuration.

Deterministic evidence covers:

- directed, negative-weight, and invalid-terminal rejection;
- empty and duplicate-singleton terminal semantics;
- a shared Steiner-center instance with worse parallel copies and deterministic
  repeat execution;
- zero-weight cycles with an acyclic zero-cost returned witness;
- disconnected terminal sets;
- an optimum exactly beyond signed-64 range; and
- a graph containing an overflowing route plus a representable cheaper route,
  proving overflow is not raised merely because some candidate is too large.

The primary differential oracle is independent of Dreyfus-Wagner and shortest
paths: 450 fixed-seed undirected multigraphs with 0-7 vertices and at most 10
logical edge copies enumerate every edge subset, test terminal connectivity with
an independent disjoint-set structure, and choose the exact minimum sum. Returned
production witnesses are replayed through `(from, adjacency_index)` against the
original `Graph`, checked for unique edge references, acyclicity, exact weight,
and terminal connectivity.

## Complexity and non-claims

Let `k` be the number of unique terminals. The direct subset DP stores
`O(2^k V)` states. Enumerating subset bipartitions costs `O(3^k V)` state-combine
work. Running one binary-heap shortest-path closure per non-empty mask adds
`O(2^k (E+V) log V)` time under the repository adjacency representation and
`O(E + 2^k V)` auxiliary/state storage, excluding the returned witness.

This is an exact fixed-parameter educational baseline, not a polynomial-time
Steiner solver. It does not claim directed Steiner tree, negative edges,
approximation algorithms, arbitrary-precision weights, prize-collecting variants,
or large-terminal practical performance.

## Scope

Exactly five files differ from the preceding merged-main checkpoint: CMake source
and test registration, one public header, one production source, one repo-native
test file, and this focused proof document. README, ROADMAP, workflow, benchmark,
recovery-authority, and the frozen compiler/backend surface remain untouched.
