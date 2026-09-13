# Scope recovery: Dulmage-Mendelsohn bipartite decomposition

## Coverage decision

A fresh post-Phase-69 live audit after online dynamic minimum spanning forest
reached merged `main` found no Dulmage-Mendelsohn / DM decomposition in live code,
pull-request history, or active branches. The repository already has maximum
bipartite matching and Konig minimum vertex cover, but no canonical structural
decomposition of a bipartite graph into underdetermined, balanced, and
overdetermined regions plus fine balanced factor-components.

This slice deliberately leaves the recent dynamic-graph streak. It reuses the
sealed Hopcroft-Karp matching surface and adds an alternating-structure proof
model rather than another update-capable graph container. The historical
compiler/backend ROADMAP remains non-authoritative under
`docs/scope_recovery_after_phase69.md`.

## Production contract

`dulmage_mendelsohn_decomposition(left_count, right_count, edges)` accepts the
repository bipartite edge abstraction and returns:

- the sealed maximum matching witness used for the decomposition;
- one coarse DM region per left/right vertex: left-deficient, balanced, or
  right-deficient;
- deterministic balanced-block IDs and explicit left/right members;
- the directed acyclic relation among distinct balanced blocks.

Parallel copies collapse to one endpoint pair for decomposition structure,
matching ordinary vertex-matching semantics. Endpoint validation fails closed.
The API does not claim canonicalization under arbitrary graph isomorphism; block
IDs are deterministic for the fixed dense vertex numbering.

## Alternating invariant and fine decomposition

For a maximum matching `M`, orient every unmatched simple edge left-to-right and
every matched edge right-to-left.

- vertices reachable from an unmatched left vertex form the left-deficient
  coarse region;
- vertices that can reach an unmatched right vertex form the right-deficient
  region;
- a maximum matching forbids overlap, because overlap would expose an
  augmenting path;
- every remaining balanced vertex is matched.

An ordinary SCC pass on this alternating graph is **not** sufficient for fine
balanced components: one isolated matched pair has only the matched `R -> L`
arc. Production therefore contracts every balanced matched pair first. Each
unmatched balanced edge then becomes a directed edge between pair vertices.
SCCs of this contracted graph are exactly the balanced factor-components; the
returned block edges are their condensation relation. Production deliberately
reuses the sealed Tarjan SCC implementation for this final structural step rather
than embedding another SCC engine.

Correctness relies on the classical Dulmage-Mendelsohn characterization of
maximum bipartite matchings and allowed edges. Tests are implementation evidence,
not a substitute for that theorem.

## Independent verification

Focused candidate verification passed with repository warning policy under GCC
and Clang and with actual GCC ASan+UBSan.

The primary oracle does not run alternating SCCs and does not use Hopcroft-Karp.
For each small graph it enumerates **every maximum matching**. An edge is marked
allowed iff it belongs to at least one maximum matching, and a vertex is marked
exposable iff some maximum matching leaves it unmatched. Connected components
of the allowed-edge graph then independently determine coarse DM regions and
balanced blocks. Edges joining distinct balanced allowed components determine
the block DAG.

Committed evidence includes deterministic left/right deficiency, a unique
perfect matching with a non-allowed inter-block edge, an alternating-cycle block,
parallel copies, invalid endpoints, every simple bipartite graph through `3 x 3`,
and 500 fixed-seed multigraphs through `4 x 4` with duplicate edge copies.

## Complexity / non-claims

Let `V = L + R` and `E` be the number of input edge copies. Endpoint-pair
normalization costs `O(E log(E+1))`; the sealed Hopcroft-Karp call contributes
`O(E sqrt(V+1))`; alternating traversals, matched-pair contraction, sealed Tarjan
SCC, and condensation are linear after normalization. The resulting direct bound
is `O(E*(sqrt(V+1)+log(E+1)) + V)` time and `O(V+E)` auxiliary/output storage.

No dynamic DM maintenance, weighted matching decomposition, canonical labeling,
minimum vertex cover enumeration, or stronger asymptotic matching bound is
claimed by this slice.
