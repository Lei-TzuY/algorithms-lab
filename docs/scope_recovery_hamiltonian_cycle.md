# Scope recovery — exact directed Hamiltonian cycle via Held-Karp

## Coverage decision

A fresh live-code, PR-history, and branch audit after exact chordal recognition
found no Hamiltonian-cycle, traveling-salesman, or Held-Karp capability. Directed
minimum arborescence is already sealed, while minimum-cycle-basis and general
isomorphism branches are occupied. This slice therefore changes proof model from
recent graph-structure recognition to exponential subset dynamic programming.

## Production contract

`minimum_directed_hamiltonian_cycle` accepts the existing directed signed-weight
`Graph` and returns the exact minimum-weight cycle rooted at vertex 0 that visits
every vertex exactly once before returning to 0.

- undirected input is rejected;
- empty and singleton directed graphs use the explicit zero-edge / zero-cost
  trivial-cycle convention;
- self-loops are ignored for nontrivial tours;
- parallel arcs are supported, with the cheapest arc used for each ordered pair
  and smallest adjacency index breaking equal-weight ties;
- returned edge witnesses contain `(from, adjacency_index, to, weight)` and can
  be replayed against the original graph;
- no cycle returns `std::nullopt`;
- the direct educational baseline rejects more than 16 vertices with
  `std::length_error` rather than pretending the exponential state space is
  practically unbounded;
- if the exact optimum is outside signed 64-bit range, `std::overflow_error` is
  thrown.

## Held-Karp invariant

For a subset `S` of non-root vertices and `v in S`, `dp[S][v]` is the exact
minimum cost of a directed path that starts at 0, visits every vertex in `S`
exactly once, visits no other non-root vertex, and ends at `v`.

Singleton states use the canonical `0 -> v` arc. The recurrence is

`dp[S union {w}][w] = min_{v in S}(dp[S][v] + cost(v,w))`.

Every Hamiltonian cycle rooted at 0 has a unique last non-root vertex `v`; adding
`v -> 0` to `dp[All][v]` therefore enumerates all feasible cycles. Conversely,
every finite closed state reconstructs one such cycle. The minimum closing state
is exact.

Parent links reconstruct the concrete vertex sequence. Equal-cost updates use a
deterministic smaller-predecessor tie rule; the API does not claim a globally
lexicographically minimum optimum tour.

## Arithmetic boundary

DP costs are not accumulated in signed 64-bit arithmetic. Production uses a
private two-limb signed-magnitude accumulator, so an overflowing non-optimal
partial/tour candidate cannot falsely reject a representable optimum. Only after
the exact optimum has been selected is it narrowed to `int64_t`.

The 16-vertex cap also bounds every exact tour sum to far less than the private
wide accumulator's representable magnitude; accumulator overflow is still
checked fail-closed.

## Complexity

For `V` vertices, canonical-pair extraction is `O(E)`. The direct Held-Karp
baseline uses `O(2^V V^2 + E)` time and `O(2^V V)` resident DP state, with
`O(V)` witness output. No polynomial-time or large-`V` practical-performance
claim is made.

## Verification

Focused candidate verification uses strict GCC, strict Clang, and actual
ASan+UBSan builds. Deterministic cases cover directed validation, trivial graphs,
no-tour input, signed/negative weights, parallel-arc tie selection, self-loops,
replay determinism, exact `INT64_MIN`, exact-optimum overflow, an overflowing
non-optimal tour beside a representable optimum, and the explicit size cap.

The primary randomized oracle is independent of Held-Karp: 450 fixed-seed
directed multigraphs with 0–8 vertices enumerate every permutation of non-root
vertices and directly scan the cheapest available arc for each consecutive
ordered pair. Production existence and exact optimum cost must match this
permutation oracle, and every returned edge witness is replayed against the
original adjacency list.
