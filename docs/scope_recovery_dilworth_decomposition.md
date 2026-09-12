# Scope recovery: Dilworth chain decomposition and maximum antichain

## Capability

`dilworth_decomposition(dag)` interprets reachability in a directed acyclic
`Graph` as a strict partial order. It returns both sides of Dilworth's theorem:

- a minimum chain decomposition covering every vertex exactly once; and
- a maximum antichain.

The common cardinality is returned as `width`. The result also exposes the
maximum bipartite-matching cardinality used by the reduction.

Undirected or cyclic inputs are rejected. Parallel arcs and edge weights do not
change reachability and therefore do not change the represented poset.

## Reduction and proof obligations

Create left and right copies of every poset element. Add `u_L -> v_R` exactly
when `u < v`, meaning `v` is reachable from `u` in the DAG. A maximum matching
in this transitive-closure bipartite graph links consecutive elements of a
minimum chain cover. Every unmatched right copy starts one reconstructed chain,
so the number of chains is

`|V| - |M|`.

The existing Hopcroft-Karp implementation also returns a Konig minimum vertex
cover `(C_L, C_R)`. The set

`A = {v : v_L not in C_L and v_R not in C_R}`

contains no comparable pair: otherwise the corresponding comparability edge
would have neither endpoint in the vertex cover. For a maximum matching/minimum
cover this antichain has exactly `|V|-|M|` elements. Thus the returned chain
cover and antichain are dual optimal witnesses of equal size, which is the
Dilworth theorem boundary used by the implementation.

The theorem is a proof obligation; randomized tests are implementation evidence,
not a finite proof of Dilworth or Konig.

## Verification

The primary test oracle does not use Hopcroft-Karp:

- Floyd-Warshall independently computes DAG reachability;
- every vertex subset is exhaustively checked for pairwise incomparability to
  compute the exact width on bounded random instances;
- every returned chain is replayed against reachability and all chains together
  must partition the vertex set;
- the returned antichain is replayed pairwise and must match the exhaustive
  optimum width;
- a dedicated regression uses the DAG `0->2, 1->2, 2->3, 2->4`, where matching
  only original arcs gives path-cover size 3 but the reachability poset has
  Dilworth width 2, proving that production must use transitive comparability;
- repeated execution must reproduce the same chains and antichain.

The committed randomized corpus uses 700 fixed-seed DAG multigraphs with zero to
nine vertices. The final candidate passed strict GCC, strict Clang, and actual
ASan+UBSan focused execution before upload.

## Complexity and non-claims

This educational baseline first validates acyclicity, then runs one graph search
from every vertex to materialize reachability. Let `R <= V^2` be the number of
strict comparable ordered pairs. The direct bounds are

- `O(V(V+E) + R sqrt(V))` time;
- `O(V^2 + R)` auxiliary/result storage, excluding the input graph.

The matching term uses the repository's Hopcroft-Karp implementation. No bitset
transitive-closure acceleration, minimum-path-cover-on-original-arcs equivalence,
weighted chain cover, arbitrary cyclic relation, or implicit-poset oracle claim
is made.

## Recovery scope

This slice deliberately changes proof model after DFA minimization rather than
extending adjacent automata or polynomial work. It reuses sealed graph and
matching capabilities while adding a poset-duality theorem boundary. The frozen
Phase-45-69 compiler/backend surface, historical `ROADMAP.md`, README, workflows,
benchmarks, and unrelated recovery records are intentionally untouched.
