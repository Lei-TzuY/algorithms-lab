# Scope recovery: Tutte-Berge min-max matching certificate

## Coverage decision

The live recovery main already contains an exact Edmonds-blossom maximum-cardinality matching solver and the freshly merged Gallai-Edmonds decomposition. A default-branch code search and pull-request-history audit found no Tutte-Berge min-max certificate surface.

This slice deliberately deepens the existing general-matching architecture instead of adding another matching algorithm. It turns one optimum matching into a replayable primal/dual-style min-max certificate: a vertex barrier, the odd components left after deleting that barrier, and the exact deficiency equality that certifies the matching cardinality.

## Production contract

`tutte_berge_certificate(const Graph&)` returns:

- the sealed maximum-cardinality matching witness inherited through Gallai-Edmonds;
- a deterministic barrier `U`, chosen as the Gallai-Edmonds attachment set `A`;
- deterministic odd and even connected components of `G-U`;
- deficiency `delta = odd(G-U) - |U|`;
- the number of unmatched vertices in the returned maximum matching;
- the underlying Gallai-Edmonds Blossom-call count.

The graph semantics are the sealed ordinary-matching semantics: input must be undirected; self-loops are irrelevant to matching/connectivity here; parallel copies collapse to one endpoint relation; stored weights are ignored.

Production fails closed with `std::logic_error` if the derived barrier would have negative deficiency, if the inherited matching cardinality exceeds the vertex bound, or if the derived deficiency disagrees with the exact unmatched-vertex count.

## Min-max proof obligation

The Tutte-Berge theorem states

`nu(G) = (|V| - max_U(odd(G-U) - |U|)) / 2`.

For the Gallai-Edmonds partition `(D,A,C)`, every `D` component is odd and factor-critical, every `C` component is even and perfectly matchable, and no `D-C` edge survives outside `A`. Therefore `U=A` attains the maximum deficiency:

`delta(G) = components(D) - |A| = |V| - 2*nu(G)`.

The implementation recomputes the connected components of `G-A` from the original graph before classifying odd/even components rather than merely relabeling `D` and `C`. The classical theorem remains the mathematical proof obligation; finite tests are evidence, not a replacement proof.

## Independent verification

The focused candidate is checked without using Blossom or Gallai-Edmonds in its primary oracle:

- enumerate every legal matching to obtain exact `nu(G)`;
- enumerate every vertex subset `U`, reconstruct `G-U`, count odd components, and maximize `odd(G-U)-|U|`;
- verify the returned barrier itself attains that independently computed maximum;
- replay the returned odd/even component partition directly on `G-U`;
- count unmatched vertices directly from the returned mate vector;
- verify `2*nu + delta = |V|` exactly.

Deterministic cases cover empty/singleton graphs, a perfect edge, odd cycle, star, disconnected mixed parity components, ignored weights, self-loops, parallel edges, directed rejection, and repeated deterministic construction. A fixed-seed randomized corpus covers 450 undirected multigraphs with 0..8 vertices and independently checks both sides of the min-max theorem.

Focused GCC strict-warning, Clang strict-warning, and actual GCC ASan+UBSan builds all pass 4/4.

## Complexity and non-claims

The direct certificate reuses the freshly sealed Gallai-Edmonds baseline, which performs `V+1` Edmonds-blossom calls. Certificate materialization adds only dense simple-adjacency/component work. Using the documented Gallai-Edmonds baseline, the conservative bound remains `O(V^3 E + V^3)` time and `O(V^2 + E)` peak working/result storage.

This slice does not claim a faster Gallai-Edmonds/Tutte-Berge algorithm, weighted matching duality, b-matching, canonical minimum barrier uniqueness, dynamic updates, or a new matching asymptotic bound.
