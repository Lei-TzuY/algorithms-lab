# Scope recovery after Phase 69

## Decision

`algorithms-lab` remains an algorithms-and-data-structures foundations laboratory.
The repository keeps the already-merged Phase 45–69 SSA/backend sequence for
historical reproducibility, but that compiler/runtime surface is frozen here and
is no longer an active conquest frontier.

The proposed Phase-70 `return_void` / fixed-frame-exit continuation was reviewed
at exact head `106d118c3bbbc4cc933f327c5d4ad94f1ac8c426`. GitHub Actions run
`34516131510` passed GCC release, Clang release, and GCC ASan+UBSan, but green CI
was not sufficient to override the repository mission. Pull request #157 was
therefore closed without merge. Its branch is retained as extraction material
for a compiler/runtime repository rather than continued inside this lab.

## Why the boundary moved

Directed dominators and dominance frontiers remain legitimate graph-algorithm
subjects. Beginning with Phase 45, however, the sequence moved from graph
analysis into compiler construction: SSA materialization/pruning/destruction,
register allocation, spilling, frame/backend lowering, retained execution
semantics, and bounded backend CFG execution. Those are coherent engineering
capabilities, but they no longer deepen the repository's stated algorithm/data-
structure mission.

This recovery does not destructively rewrite history. Existing merged code and
its tests stay buildable. The governance change is prospective: new work must
again pass the repository-specific test of substantial algorithmic or data-
structural depth.

## Recovered frontier

The first recovered executable slice is exact planar closest-pair search over the
existing bounded-integer `Point2i` geometry domain. It adds a classical
`O(n log n)` divide-and-conquer algorithm, deterministic witness semantics, and
an independent quadratic differential oracle. This is intentionally a return to
algorithmic depth rather than a documentation-only reset.

After that checkpoint reached exact merged-main green, the second recovered
slice advances the same geometry foundation through a different algorithmic
paradigm: exact planar diameter. It reuses the sealed monotone-chain convex hull
and then enumerates antipodal hull pairs with rotating calipers, returning a
canonical farthest-pair witness and verifying the complete result against an
independent quadratic all-pairs oracle.

After the planar-diameter checkpoint also reached exact merged-main green, the
third recovered slice moves from point-set extremal geometry to area aggregation:
exact axis-aligned rectangle union area via an x-sweep, y-coordinate compression,
and a first-principles covered-length segment tree. It adds a genuine sweep-line
paradigm with exact bounded-integer area arithmetic and independent small-grid
union-area verification rather than another distance-query variant.

After the rectangle-union checkpoint reached exact merged-main green, the next
recovered slice leaves the geometry streak rather than farming adjacent polygon
helpers. It targets deterministic exact weighted global minimum cut via the
Stoer-Wagner maximum-adjacency/contraction algorithm. The slice reuses the
existing undirected-capacity edge abstraction, but not max-flow as its production
engine; exhaustive bipartition remains the primary oracle, while the sealed
Gomory-Hu tree supplies secondary cross-implementation evidence. This is a
deliberate contrast among direct deterministic contraction, repeated-flow
cut-equivalent trees, and the sealed randomized unweighted Karger contract.

After the Stoer-Wagner checkpoint reached exact merged-main green, a fresh
coverage audit found that the repository still had only single-source
Dijkstra/Bellman-Ford shortest paths and no Johnson/APSP implementation. The
next recovered slice therefore targets Johnson all-pairs shortest paths: global
Bellman-Ford potentials, exact non-negative reweighting, one heap-Dijkstra pass
per source, and an independent Floyd-Warshall primary oracle. This deliberately
leaves the cut frontier instead of farming another min-cut variant.

After Johnson APSP reached exact merged-main green, another coverage audit moved
away from shortest-path variants and found a more basic graph-structure gap:
the repository had no articulation-point, bridge, or bridge-component low-link
analysis. The next recovered slice therefore adds an iterative Tarjan low-link
analysis over the existing undirected multigraph `Graph`. It distinguishes
parallel edge copies with internal edge identities, treats self-loops correctly,
returns bridge and articulation witnesses plus deterministic bridge-component
labels, and verifies all three outputs against independent edge/vertex-removal
connectivity oracles. This recovers a classical DFS proof obligation rather than
extending the recent geometry, cut, or APSP streak.

After low-link analysis reached exact merged-main green, recovery deliberately
left adjacent DFS variants and moved to Boolean constraint solving. Exact 2-SAT
builds the canonical implication graph, reuses the sealed SCC decomposition,
returns either a satisfying assignment or replayable contradiction paths, and
uses exhaustive truth-table enumeration as its primary oracle rather than SCC
self-verification.

After 2-SAT reached exact merged-main green, the next recovery step again changed
proof model: exact Aho-Corasick matching over arbitrary bytes. It adds a
first-principles multi-pattern trie/failure automaton with duplicate-pattern and
empty-pattern semantics, while an independent boundary-by-boundary naïve oracle
checks every match witness. It does not extend the frozen backend or the sealed
BWT family.

After Aho-Corasick reached exact merged-main green, a fresh coverage audit found
no Eulerian-trail / Hierholzer capability in the repository. The next recovered
slice therefore targets exact edge-covering trails and circuits on the existing
`Graph` multigraph abstraction: directed and undirected degree/connectivity
conditions, explicit logical-edge identities for parallel edges and self-loops,
and first-principles Hierholzer traversal. Small randomized instances are checked
against an independent exhaustive search over `(current vertex, used-edge mask)`
rather than by repeating the production feasibility criteria. This adds a new
edge-decomposition proof model instead of farming another shortest-path, cut, or
string-index variant.

After the Eulerian-trail checkpoint reached exact merged-main green, recovery
again moved away from adjacent graph/string/cut variants. A fresh coverage audit
found no integer-factorization capability even though the sealed number-theory
foundation already provides Euclidean GCD, overflow-safe modular arithmetic, and
deterministic full-`uint64_t` primality. The next recovered slice therefore adds
exact full-width prime factorization through replayable Pollard-Rho splitting.
Randomness affects the search path and runtime only: every returned factor is
certified prime, the sorted factor multiset multiplies back to the input, bounded
inputs are checked against independent trial division, and full-width known
factorizations cover adversarial arithmetic. No deterministic Pollard-Rho runtime,
cryptographic-randomness, or probabilistic-correctness claim is implied.

After exact factorization reached exact merged-main green, recovery again changed
proof model. A fresh coverage audit found no exact-cover capability. The next
slice therefore implements Knuth's Algorithm X over a first-principles Dancing
Links sparse matrix. Cover/uncover is an explicitly reversible structural
obligation; minimum-column choice is a deterministic search heuristic only.
Returned row witnesses are replayed against every column, and small randomized
instances are checked against independent exhaustive enumeration of row subsets.
This deliberately leaves number theory again rather than farming arithmetic
variants.

## Prospective frontier authority

`ROADMAP.md` still contains historical presentation drift from the frozen
compiler/backend excursion (for example an old backend phase can appear as an
active frontier even though later backend phases were already merged and then
prospectively frozen here). That stale heading must not be used to resume
compiler/runtime conquest.

Until the historical roadmap is deliberately normalized without rewriting
repository history, prospective work is governed by this recovery decision plus
a fresh live-state / architecture-coverage audit. New slices must add substantial
algorithmic or data-structural depth, executable behavior, proof obligations,
and independent verification; green CI alone is not a reason to continue an
out-of-scope subsystem.
