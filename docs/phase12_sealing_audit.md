# Phase 12 sealing audit

Phase 12 is sealed after the deterministic minimum-vertex-cover approximation reached merged `main` and the exact merged-main GCC, Clang, and ASan/UBSan matrix completed successfully. The phase is sealed on the quality of its approximation contract rather than by accumulating multiple naming-adjacent approximation algorithms.

## Capability covered

- a feasible minimum-vertex-cover witness on the repository's undirected multigraph abstraction;
- explicit mandatory handling of self-loop vertices rather than silently assuming simple-graph inputs;
- a replayable residual maximal-matching witness;
- an `O(V + E)` constructive implementation whose edge weights are explicitly outside the cardinality objective;
- an approximation factor justified by a lower-bound witness rather than empirical averages.

## Approximation evidence

Let `F` be the forced self-loop vertices and `M` the maximal matching built after edges incident to `F` are already covered. Every optimum contains all of `F`, and because edges in `M` are vertex-disjoint and avoid `F`, every optimum needs at least one additional endpoint per matching edge. Therefore `OPT >= |F| + |M|`, while the implementation returns exactly `|F| + 2|M| <= 2 OPT`.

The universal ratio comes from this proof obligation. Tests independently enumerate all vertex subsets on small inputs to obtain the exact optimum and verify the implementation's cover, witness lower bound, result-size identity, and factor-two inequality. Six hundred fixed-seed multigraphs plus deterministic clique/star/self-loop/parallel-edge cases passed focused GCC/Clang/ASan verification and the full repository CI.

## Architecture assessment

A second Phase-12 algorithm is not justified merely to make the phase plural. Set-cover, metric-TSP, or other approximation algorithms would introduce different problem domains but are not required to establish the repository's missing correctness boundary: feasible output plus a proof-derived quality guarantee checked against independent exact small instances. Adding one now would favor breadth over conceptual density.

## Promotion

The next substantial uncovered frontier is amortized analysis through a genuinely self-adjusting data structure. Phase 13 starts with an ordered splay-tree set: searches, insertions, and erasures restructure the tree through zig/zig-zig/zig-zag rotations; individual operations may cost `O(n)`, while the access lemma supplies the amortized logarithmic boundary. Randomized differential tests will compare set semantics with `std::set` while structural checks replay BST ordering, parent links, node count, and access-to-root behavior.
