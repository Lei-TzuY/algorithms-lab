# Scope recovery: exact weighted minimum cycle basis

## Recovered capability

This slice adds an exact minimum-weight cycle basis for the repository's undirected positive-weight multigraph `Graph` abstraction.

The public result preserves a deterministic logical-edge table. Parallel edge copies and self-loops are distinct logical edges; every returned cycle is a sorted GF(2) edge vector over those logical ids. The result also exposes the cycle-space dimension and the exact total basis weight when that value is representable in `uint64_t`.

Directed graphs and non-positive weights are rejected. This is an intentional domain boundary for the Horton shortest-path candidate theorem used here, not a claim about cycle bases in more general weighted graphs.

## Algorithm and proof obligations

For every root vertex, production constructs one deterministic shortest-path tree. For every logical edge `(u,v)`, it forms the Horton vector

`P(root,u) xor P(root,v) xor {(u,v)}`.

A non-zero vector has even incidence parity and therefore lies in the undirected cycle space. Self-loops naturally contribute one-edge vectors; parallel copies remain distinct coordinates.

The candidate family is sorted by `(cycle weight, logical edge-id vector)`. GF(2) Gaussian elimination then greedily accepts a candidate iff it increases rank. The correctness boundary is the combination of two facts:

1. Horton's shortest-path-tree candidate family contains a minimum-weight cycle basis for a positive-weight undirected graph.
2. Linear independence of candidate edge vectors forms a matroid, so the standard ascending-weight greedy rule returns a minimum-weight independent basis from that family.

The required rank is checked independently from graph structure as `m - (n - c)`, where `m` is the number of logical edges and `c` is the number of connected components. A result that cannot reach that rank fails closed.

## Representability

Each original `Graph::Weight` is positive and therefore converts exactly to `uint64_t`. Shortest-path additions and candidate-cycle additions are checked. An overflowing path/candidate is not wrapped and is not materialized. If the exact minimum basis total cannot be represented in `uint64_t`, the operation throws `std::overflow_error` rather than returning a modular or saturated value.

No arbitrary-precision arithmetic claim is made.

## Verification

Focused verification passes with:

- GCC C++20 under the repository strict warning set,
- Clang C++20 under the same warning set,
- GCC ASan + UBSan.

Deterministic cases cover forests, a weighted triangle/square-with-diagonal, disconnected components, self-loops, parallel edges, directed rejection, non-positive-weight rejection, deterministic replay, and an unrepresentable basis total.

The randomized primary oracle is structurally independent from Horton generation. For 450 fixed-seed multigraphs with at most 6 vertices and 10 logical edges, the test enumerates every non-zero edge subset, keeps exactly the even-incidence cycle-space vectors, sorts that complete ground set by exact weight, and performs an independent GF(2) minimum-basis greedy pass. Production total weight, rank, witness parity, witness weight, and replay determinism must match that exhaustive oracle.

## Complexity and non-claims

This is deliberately a proof-oriented baseline, not the fastest known MCB implementation. It uses an `O(V^2 + E)` array-based shortest-path pass for each root, materializes up to `O(VE)` Horton candidates, and performs explicit bit-vector elimination. The direct worst-case elimination cost can be cubic in `E` up to machine-word factors.

This slice does **not** claim:

- directed minimum cycle basis,
- negative or zero-weight support,
- arbitrary-precision total weights,
- de Pina / Kavitha-style asymptotic improvements,
- dynamic cycle-basis updates,
- benchmark superiority over specialized libraries.

The recovered depth is the exact interaction between shortest-path candidate structure, GF(2) cycle-space rank, multigraph edge identity, and independently verified minimum-weight linear-basis selection.
