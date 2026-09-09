# Roadmap

This roadmap is directional, not a completeness claim. Promotion happens only after the previous checkpoint has executable behavior, documented proof obligations/invariants, deterministic adversarial tests, and appropriate randomized/differential evidence.

## Phase 1 — foundations and graph search — SEALED

- [x] Binary search, merge sort, quicksort
- [x] Heap / priority queue and disjoint-set union
- [x] Graph representation, BFS, DFS, reachability/components, cycle detection, topological sort
- [x] Dijkstra and Bellman-Ford with differential verification

## Phase 2 — greedy and deeper graph structure — SEALED

- [x] Greedy algorithm pattern: interval scheduling with exchange argument and exhaustive small-instance oracle
- [x] Minimum spanning forests: Kruskal and Prim with differential verification
- [x] Strongly connected components: Tarjan and Kosaraju with partition/reachability verification
- [x] Advanced graph traversal integration: SCC condensation DAG verified through Phase-1 topological sort

## Phase 3 — dynamic programming — SEALED

- [x] 0/1 and unbounded knapsack with reconstruction and independent randomized oracles
- [x] Longest increasing subsequence with quadratic-vs-nlogn differential verification and reconstruction
- [x] Edit distance with deterministic edit-script reconstruction and metric/property verification
- [x] Interval DP: matrix-chain multiplication with reconstructable split plans and exhaustive small-instance verification
- [x] Tree DP: maximum-weight independent set with take/skip reconstruction and exhaustive small-tree verification

## Phase 4 — range-query and structural data structures — SEALED

- [x] Fenwick tree with transactional checked updates and randomized naïve-array differential verification
- [x] Segment tree with transactional point assignment and Fenwick/naïve cross-structure differential verification
- [x] Sparse table with immutable O(1) RMQ, deterministic argmin, and exhaustive randomized naïve verification
- [x] Byte trie with multiset prefix counts and randomized map/prefix-scan differential verification
- [x] Rollback DSU with reversible snapshots and randomized rebuilt-graph differential verification

## Phase 5 — string algorithms — SEALED

- [x] KMP with explicit prefix/failure state, overlap-aware all-match search, and naïve differential verification
- [x] Z-function with a rightmost half-open Z-box and naïve LCP differential verification
- [x] Rolling hash with double modular substring fingerprints and direct-polynomial differential verification
- [x] Suffix array with prefix-doubling order, inverse rank, Kasai LCP, and naïve suffix/LCP differential verification

## Phase 6 — advanced graph/offline algorithms — SEALED

- [x] Max flow / min cut: Dinic flow, per-edge witness, residual source-side cut, Edmonds-Karp + exhaustive-cut differential verification
- [x] Bipartite matching: Hopcroft-Karp matching witness, König minimum vertex cover, exhaustive + Dinic reduction verification
- [x] Lowest common ancestor: strict rooted-tree validation, binary lifting, distance/k-th ancestor queries, naïve differential verification
- [x] Offline dynamic connectivity: edge-active intervals, segment tree over time, rollback DSU, naïve temporal differential verification

## Phase 7 — selected advanced foundations — SEALED

- [x] Computational geometry foundations: exact bounded-integer predicates, segment intersection, and convex hull
- [x] Number theory foundations: Euclidean GCD, overflow-safe modular arithmetic, deterministic uint64 primality
- [x] Selected advanced integration: heavy-light decomposition over `Graph` + Phase-4 `SegmentTree` with mutable path/subtree sums

## Phase 8 — algebraic transforms and polynomial algorithms — SEALED

- [x] Radix-2 NTT and polynomial convolution over `998244353` with transform round-trip and naïve-convolution differential verification
- [x] Exact bounded integer convolution via multiple NTT primes + CRT with an explicit coefficient/representability bound
- [x] Formal power series inverse via Newton doubling over `998244353` with independent recurrence verification

## Phase 9 — weighted combinatorial optimization — SEALED

- [x] Minimum-cost maximum flow with signed costs, checked flow/cost certificates, residual-potential optimality evidence, and exhaustive small-flow differential verification
- [x] Assignment problem via Hungarian algorithm with exhaustive small-instance verification and a min-cost-flow reduction as cross-implementation integration evidence
- [x] Lower-bounded / demand min-cost circulation with explicit feasibility transformation and independent small-instance verification

## Phase 10 — general graph matching — SEALED

- [x] Maximum-cardinality matching in arbitrary undirected graphs via Edmonds blossom, with exhaustive small-graph verification and Hopcroft-Karp equality on bipartite instances

## Phase 11 — randomized algorithms and probabilistic contracts — SEALED

- [x] Karger-style randomized contraction for undirected global minimum cut with explicit seed/trial semantics, replayable cut witness, exhaustive small-graph optimum checks, and no unsupported finite-trial exactness claim

## Phase 12 — approximation algorithms and executable quality guarantees — SEALED

- [x] Deterministic 2-approximation for minimum vertex cover via maximal matching, with mandatory self-loop vertices, replayable matching/cover witness, exhaustive small-graph optimum checks, and explicit ratio validation

## Phase 13 — amortized self-adjusting data structures — SEALED

- [x] Splay-tree ordered set with zig/zig-zig/zig-zag rotations, access-to-root behavior, insertion/erasure, explicit structural invariants, randomized `std::set` differential verification, and an honest amortized-vs-worst-case complexity boundary

## Phase 14 — persistent and versioned data structures — SEALED

- [x] Persistent segment tree with immutable branching versions, `O(log n)` path-copy point assignment, structural sharing evidence, transactional failed-update behavior, checked `int64_t` range sums, and randomized version-DAG differential verification

## Phase 15 — online algorithms and competitive analysis — SEALED

- [x] Deterministic LRU paging with replayable hit/fault/eviction trace, independent exact offline optimum on bounded sequences, and an explicit theorem-derived competitive-ratio boundary

## Phase 16 — streaming and sublinear state — SEALED

- [x] Misra-Gries heavy hitters with one-pass `O(k)` state, an executable cancellation witness, deterministic additive frequency-error guarantees, exhaustive bounded-stream verification, and exact-frequency differential checks

## Phase 17 — packed static rank/select indexing — SEALED

- [x] Immutable packed bit-vector with exact half-open `rank0/rank1`, deterministic `select0/select1`, explicit packed-storage accounting, word-boundary adversarial cases, and randomized naïve differential verification

## Phase 18 — byte wavelet matrix static sequence indexing — SEALED

- [x] Immutable byte wavelet matrix built from eight Phase-17 packed rank/select levels, with exact `access`, prefix/range `rank`, global `select`, range `kth_smallest`, explicit storage accounting, and independent naïve differential verification

## Phase 19 — exact BWT backward-search text indexing — SEALED

- [x] Exact byte-string substring count/locate via conceptual-sentinel BWT backward search, reusing Phase-5 suffix-array order and Phase-18 wavelet-matrix occurrence counts, with arbitrary-byte support and independent direct-scan verification

## Phase 20 — sampled BWT locate space/time tradeoff — SEALED

- [x] Replace the full suffix-row position table with packed sampled-row membership plus periodic suffix-position samples, reconstructing exact locate positions by bounded LF walks with explicit sample-rate/storage/query-cost evidence

## Phase 21 — run-length BWT occurrence representation — SEALED

- [x] Replace the resident full BWT byte wavelet occurrence structure with a run-length byte rank/access index, preserving exact count/locate semantics while exposing BWT run-count/storage/query-cost evidence and making no universal compression claim

## Phase 22 — run-aware BWT toehold locating — SEALED

- [x] Maintain one exact suffix-position toehold through backward search using BWT run-boundary suffix samples, so a non-empty matched interval can return one exact occurrence from `O(R)` run-aware sampling state without reintroducing a full suffix-row position table; preserve arbitrary-byte/conceptual-sentinel semantics and verify every returned witness against direct scan

## Phase 23 — run-boundary-sampled full BWT locating — SEALED

- [x] Enumerate every occurrence in an exact backward-search interval without consulting Phase-20 periodic locate samples: resolve each matched BWT row by LF-walking until reaching either the conceptual sentinel or a Phase-22 run-boundary sample, reconstruct the exact suffix position modulo `n+1`, preserve arbitrary-byte semantics, expose the intentionally slower worst-case LF-walk bound, and verify complete sorted locate output against independent direct scan. This is an `O(R)` resident-sampling tradeoff, not a claim of r-index-optimal locate bounds.

## Phase 24 — query-local memoized run-sampled BWT locating — SEALED

- [x] Reuse the sealed Phase-22 `O(R)` resident run-boundary suffix samples while adding only query-local `O(n)` row-position memoization: seed the conceptual sentinel and run-boundary samples, path-compress LF walks as rows are resolved, enumerate the complete exact match set, expose LF-step/cache diagnostics, prove each conceptual row is newly traversed at most once per query, and verify exact output against direct scan. The sealed conservative bound is `O(m log(R+1) + n log(R+1) + occ log occ)` locate time with `O(n + occ)` query workspace and no r-index or compressed-construction claim.

## Phase 25 — bidirectional BWT exact interval extension — SEALED

- [x] Maintain coordinated exact suffix-array intervals for a pattern in the forward text and its reversal in the reversed text, support arbitrary-byte extension on either the left or the right from one common empty state, preserve paired interval cardinality invariants, and verify every extension sequence/count against independent direct scan. The sealed first-principles bounded-byte-alphabet baseline performs explicit alphabet work per extension, reuses the sealed BWT occurrence machinery, and makes no r-index, approximate-matching, compressed-construction, or asymptotically optimal bidirectional-index claim.

## Phase 26 — bounded-substitution approximate BWT search — SEALED

- [x] Exact Hamming-distance substring search for fixed-length arbitrary-byte patterns with an explicit substitution budget, built from the sealed Phase-25 bidirectional extension state. Production prunes empty exact intervals, exposes deterministic search-state/branch diagnostics, and returns exact match positions without using a direct scan. Verification compares against an independent direct-scan Hamming oracle. The sealed baseline deliberately excludes insertion/deletion edit distance, heuristic seeds, probabilistic claims, and hidden polynomial-time guarantees: its first-principles state expansion may depend exponentially on mismatch budget/alphabet branching and states that cost explicitly.

## Phase 27 — bounded edit-distance approximate BWT search — SEALED

- [x] Exact substring start-position search under a bounded byte-oriented Levenshtein budget, extending the sealed BWT state machine with substitution, insertion, and deletion transitions. Production retains deterministic replayable diagnostics, deduplicates dominated equivalent search states without direct scanning, reconstructs exact start-position witnesses through the sealed BWT locate machinery, and makes the exponential-in-budget baseline cost explicit. Verification compares against an independent direct dynamic-programming substring oracle. No seed-and-extend, affine-gap, probabilistic, or polynomial-time approximate-index claim is implied.

## Phase 28 — bounded affine-gap approximate BWT search — SEALED

- [x] Exact substring start-position search under a bounded byte-oriented affine-gap score, extending the sealed Phase-27 state machine with explicit gap-open versus gap-extend memory for insertion and deletion runs. Production preserves deterministic replayable diagnostics, exact BWT-only witness reconstruction, domination keyed on the full scoring state, and explicit exponential baseline cost. Verification compares against an independent affine-gap dynamic-programming substring oracle, with `{1,1,1}` scoring cross-checked against sealed Phase 27. No seed-and-extend, probabilistic, polynomial-time approximate-index, or asymptotically optimal compressed-search claim is implied.

## Phase 29 — BWT text reconstruction and bounded extraction substrate — SEALED

- [x] Reconstruct exact source bytes from the conceptual-sentinel BWT/LF state without retaining a separate source-text copy, then expose validated half-open bounded extraction over arbitrary bytes with deterministic LF-step diagnostics. Verification compares full reconstruction and randomized slices against the constructor input while keeping resident state on the sealed BWT representation. The sealed baseline intentionally uses linear reconstruction/query workspace and exposes that cost; it is a correctness substrate rather than a compressed-optimal extraction or universal performance claim.

## Phase 30 — exact seed-and-verify bounded edit-distance search — SEALED

- [x] Generate complete candidate start positions from `k+1` non-overlapping exact pattern seeds located through the sealed BWT index, account explicitly for the `±k` start displacement induced by preceding insertions/deletions, reconstruct the source text once through the sealed Phase-29 LF substrate, and verify each unique candidate with exact bounded Levenshtein dynamic programming. Verification compares against an independent direct-DP substring oracle and cross-checks bounded cases with the sealed Phase-27 state-expansion search. The implementation exposes seed-hit/candidate/reconstruction/verification diagnostics and makes its candidate-sensitive polynomial verification cost explicit; no heuristic seed, universal speedup, compressed-optimal extraction, or hidden exactness assumption is claimed.

## Phase 31 — periodic-sample local BWT extraction — SEALED

- [x] Add a suffix-position-to-row inverse over the sealed Phase-20 periodic locate samples, then expose exact half-open local text extraction that starts from the nearest sampled suffix position at or after the requested end and LF-walks backward only far enough to fill the requested range. Preserve arbitrary-byte/conceptual-sentinel semantics, expose LF-step and resident inverse-sample payload diagnostics, verify randomized slices against direct source substrings across varied sample rates, and make the explicit `O((end-begin)+sample_rate)` LF-step / `O(end-begin)` output-space tradeoff executable. This is a bounded local-extraction improvement, not a compressed-optimal random-access or universal speedup claim.

## Phase 32 — candidate-local exact seed-and-verify — SEALED

- [x] Preserve the sealed Phase-30 `k+1` exact-seed candidate-completeness argument while replacing its unconditional full-text reconstruction with exact candidate-local verification windows extracted through the sealed Phase-31 periodic-sample extractor. Keep exact bounded Levenshtein start-position semantics, expose window count/bytes/LF-step and candidate diagnostics, compare results against both the sealed Phase-30 implementation and an independent direct-DP substring oracle across arbitrary bytes and varied locate sample rates, and include a sparse-candidate case whose measured extraction work is below the full-reconstruction baseline. This is candidate-sensitive exact verification, not a universal speedup or compressed-optimal approximate-search claim.

## Phase 33 — coalesced candidate-window exact verification — SEALED

- [x] Preserve Phase-32 exact candidate generation and bounded Levenshtein verification semantics while sorting candidate windows and coalescing overlapping or touching source intervals before extraction. Extract each merged interval once through the sealed Phase-31 periodic-sample extractor, verify every covered candidate from an offset into that shared buffer, expose original-candidate versus merged-window counts plus extracted-byte/LF-step diagnostics, and compare exact positions against both sealed Phase 32 and an independent direct-DP substring oracle across arbitrary bytes and varied locate sample rates. Include a clustered-candidate regression whose merged-window count and measured LF work are strictly below the Phase-32 per-candidate baseline. This is batching of exact local verification work, not a universal speedup or compressed-optimal approximate-search claim.

## Phase 34 — dynamic forests and link-cut trees — SEALED

- [x] Implement a first-principles link-cut tree over a dynamic forest with exact `link`, `cut`, connectivity, and path edge-distance queries. Distinguish represented-tree topology from auxiliary splay structure, reject cycle-creating links and cuts of non-edges, expose the standard amortized rather than worst-case-per-operation complexity boundary, and verify long randomized operation traces against an independent naïve forest/BFS path oracle. Do not hide the core dynamic-tree machinery behind a library implementation.

## Phase 35 — augmented dynamic trees — SEALED

- [x] Extend the sealed link-cut forest with signed node values, point assignment, and exact represented-path sums. Maintain aggregate state through access, splay rotations, and lazy reversal; keep internal aggregate arithmetic exact across every representable repository-sized path and reject only public `int64_t` results that are genuinely out of range. Verify long mixed topology/value/query traces against an independent naïve forest/BFS path-sum oracle, including cancellation and overflow boundaries. Preserve the standard amortized rather than worst-case-per-operation link-cut complexity contract.

## Phase 36 — lazy augmented dynamic trees — SEALED

- [x] Add uniform represented-path assignment to the sealed augmented link-cut forest. Compose a pending numeric assignment tag correctly with access/splay and lazy reversal, update exposed-path aggregates exactly as path length times the assigned `int64_t` value without narrowing intermediate state, and preserve point assignment, topology operations, and exact path-sum semantics. Verify long mixed topology/point-assignment/path-assignment/query traces against an independent naïve forest/BFS oracle, including overwrite order, deferred-tag exposure, cancellation, and public overflow boundaries. Preserve the standard amortized rather than worst-case-per-operation link-cut complexity contract.

## Phase 37 — affine lazy represented-path updates — SEALED

- [x] Add uniform represented-path addition to the sealed lazy link-cut forest. Compose addition correctly with pending assignment and lazy reversal, maintain the exact wide path aggregate and enough exposed-path value-range evidence to reject transactionally before any affected individual node would leave `int64_t`, and preserve point assignment, path assignment, topology, path distance, and path-sum semantics. Verify deterministic assignment/addition order, per-node `INT64_MIN`/`INT64_MAX` boundaries, cancellation, topology changes with pending tags, and long fixed-seed mixed traces against an independent eager naïve forest/BFS oracle. Preserve the standard amortized rather than worst-case-per-operation link-cut complexity contract.

## Phase 38 — ordered represented-path navigation — SEALED

- [x] Add zero-based `kth_vertex_on_path(first, second, rank)` to the sealed link-cut forest. After represented-path exposure, the auxiliary splay in-order sequence must equal the unique path order from `first` to `second`; rank selection must push deferred reversal/assignment/addition state while descending via stored auxiliary subtree sizes, reject disconnected endpoints and out-of-range ranks, preserve represented topology and numeric semantics, and splay the selected vertex so the standard sequence-level amortized `O(log V)` bound remains the claimed boundary rather than a worst-case-per-operation guarantee. Verify forward/reverse paths, singleton and boundary ranks, pending lazy numeric tags, cut/relink topology changes, and long fixed-seed mixed traces against an independent BFS path oracle.

## Phase 39 — rooted virtual-subtree accounting — SEALED

- [x] Add exact `rooted_subtree_vertex_count(root, vertex)` to the sealed link-cut forest by maintaining represented-size contributions from virtual children as preferred paths change. `access` must transfer the old preferred right child into virtual accounting and the newly preferred child out of it; `link`/`cut` must preserve the same represented-cardinality invariant. After `make_root(root)` and `access(vertex)`, the query must count `vertex` plus all rooted descendants while excluding the exposed ancestor path, reject disconnected root/vertex pairs, preserve all sealed topology/path-update/path-navigation semantics, and retain the standard sequence-level amortized `O(log V)` bound rather than claiming worst-case `O(log V)`. Verify chains, stars, root changes, cut/relink, pending lazy numeric tags, and long fixed-seed mixed traces against an independent BFS-rooted subtree oracle.

## Phase 40 — rooted represented-subtree numeric aggregation — SEALED

- [x] Add exact `rooted_subtree_sum(root, vertex)` to the sealed link-cut forest by extending Phase-39 virtual-child accounting with exact numeric contributions. `access` must transfer both represented cardinality and represented value aggregates when preferred children change; `link`/`cut` must preserve both invariants. Point assignment and lazy represented-path assignment/addition must update only values on the auxiliary path while leaving virtual descendants unchanged, so represented numeric aggregates must account for the difference between auxiliary-node values and virtual-subtree values rather than scaling the whole represented subtree. After `make_root(root)` and `access(vertex)`, subtract the exposed ancestor-side represented value aggregate to obtain the exact rooted subtree sum. Return the value when representable as `int64_t`, throw `std::overflow_error` only when the final exact result is genuinely outside `int64_t`, preserve all sealed topology/cardinality/path-update/path-navigation behavior, and retain the standard sequence-level amortized `O(log V)` bound. Verify positive/negative cancellation, exact overflow boundaries, chains/stars/root changes, cut/relink, pending assignment/addition tags, and long fixed-seed mixed traces against an independent BFS-rooted subtree-sum oracle.

## Phase 41 — Gomory-Hu all-pairs minimum-cut trees — SEALED

- [x] Build an exact Gomory-Hu cut-equivalent tree for an undirected non-negative-capacity multigraph by reusing the sealed Dinic max-flow implementation for the standard `V-1` s-t cut computations. Preserve parallel-edge multiplicity, ignore self-loops as cut-neutral, reject invalid endpoints or negative capacities, and propagate representability failures rather than wrapping total cut values. Expose the deterministic parent/cut-value tree plus validated pairwise minimum-cut queries whose answer is the minimum edge value on the unique tree path. Verify every tree edge and every pairwise query on small fixed-seed multigraphs against an independent exhaustive s-t cut oracle, including disconnected graphs and zero-capacity cuts. State the Gomory-Hu cut-equivalence theorem as a proof obligation rather than inferring it from tests; claim only the implemented `V-1` max-flow construction cost plus explicit tree-index/query costs, with no hidden asymptotic improvement claim.

## Phase 42 — directed minimum arborescence and cycle contraction — SEALED

- [x] Implement an exact minimum-cost arborescence rooted at a specified vertex in a directed signed-weight multigraph via Chu-Liu-Edmonds. Validate endpoints/root and reject instances where some vertex is unreachable from the root; preserve parallel edges and ignore self-loops as unusable arborescence edges. Select deterministic minimum incoming edges, contract directed cycles with correctly adjusted costs, expand contractions into a reconstructable original-edge witness, and compute the exact signed total cost with checked representability. Verify fixed-seed small graphs against an independent exhaustive rooted-arborescence oracle, including negative edges, ties, parallel edges, unreachable vertices, nested contractions, and total-cost boundaries. Treat the Chu-Liu-Edmonds contraction theorem as a proof obligation rather than a test-derived claim, and state only the implemented construction complexity without hiding cycle-expansion costs.

## Phase 43 — directed dominator trees and control-flow dominance — SEALED

- [x] Implement exact immediate dominators from a specified start vertex in a directed multigraph with deterministic DFS numbering, explicit reachable/unreachable semantics, and a first-principles semi-dominator/link-eval construction. Expose a reconstructable immediate-dominator tree plus exact `dominates(u,v)` behavior on the reachable subgraph; accept parallel edges and self-loops without changing dominance semantics and reject invalid starts/endpoints or undirected input if the `Graph` abstraction is used. Verify chains, diamonds, cycles, irreducible merge shapes, unreachable vertices, parallel edges, and self-loops, then compare fixed-seed small directed graphs against an independent iterative dominator-set fixed-point oracle. Treat the Lengauer-Tarjan dominator theorem and link-eval invariants as proof obligations rather than test-derived claims, and state only the complexity actually provided by the chosen first-principles implementation without importing a stronger optimized bound.

## Phase 44 — dominance frontiers and SSA phi-placement substrate — SEALED

- [x] Build exact per-block dominance frontiers on the start-reachable directed CFG using the sealed Phase-43 dominance index, then expose deterministic iterated-dominance-frontier closure for a set of reachable definition blocks as a phi-placement substrate. Preserve parallel-edge/self-loop semantics without duplicating frontier entries, reject invalid or unreachable definition blocks explicitly, and make the reachable-only contract deterministic. Verify frontier membership against an independent definition-based oracle (`x` dominates some predecessor of `y` while `x` does not strictly dominate `y`) and verify IDF closure independently rather than by copying the production recurrence. Keep post-dominators, control dependence, full SSA renaming, and incremental CFG maintenance outside this slice.

## Phase 45 — minimal SSA phi materialization and version renaming — SEALED

- [x] Construct deterministic SSA over a start-reachable directed CFG using the sealed Phase-43 dominator tree and Phase-44 IDF substrate: accept a block-ordered variable event IR, materialize phi nodes at iterated-frontier blocks, then rename uses and definitions along the dominator tree from explicit version-0 entry values. Require a predecessor-free entry block, deduplicate parallel predecessor blocks, keep unreachable blocks explicitly outside the SSA domain, emit replayable phi-incoming/version witnesses, and reject malformed variable ids or block shapes. Verify diamonds, loops, parallel edges, statement ordering, and fixed-seed reachable DAGs against an independent reaching-definition/topological oracle rather than the production IDF recurrence. Keep liveness-pruned/semi-pruned SSA, MemorySSA, optimization passes, post-dominators/control dependence, and incremental CFG maintenance outside this slice.

## Phase 46 — liveness-pruned SSA phi placement — SEALED

- [x] Compute deterministic per-variable live-in/live-out state on the start-reachable directed CFG from the sealed Phase-45 event IR, then construct pruned SSA by materializing an IDF candidate phi only when that variable is live-in at the candidate block and propagating a phi block as a new definition site only when the phi is actually materialized. Reuse the sealed Phase-45 version-renaming and phi-incoming semantics rather than inventing a second SSA format. Verify dead-join phi elimination, retained live joins and loop-carried values, local use/definition ordering, parallel-edge/unreachable semantics, and fixed-seed graph corpora against an independent liveness oracle plus independent reaching-definition identities. Require the pruned phi set to be a subset of minimal SSA and every retained phi to be live-in. Keep semi-pruned SSA, MemorySSA, SSA destruction, optimization passes, post-dominators/control dependence, and incremental CFG maintenance outside this slice.

## Phase 47 — out-of-SSA lowering and phi elimination — SEALED

- [x] Lower deterministic scalar SSA into a non-SSA edge-copy form by eliminating phi nodes with predecessor-specific parallel-copy bundles, splitting critical edges when copies cannot be placed unambiguously, and scheduling parallel copies without clobbering cyclic assignments. Preserve the sealed reachable/unreachable, version identity, predecessor-deduplication, and deterministic ordering contracts while emitting replayable lowering provenance. Verify diamonds, loop-carried swaps, critical-edge splitting, parallel predecessor edges, unreachable blocks, and fixed-seed CFGs against an independent execution/renaming oracle. Keep MemorySSA, optimization passes, post-dominators/control dependence, register allocation/coalescing, and incremental CFG maintenance outside this slice.

## Phase 48 — phi-free virtual-register allocation — SEALED

- [x] Compute exact may-liveness over the sealed phi-free `OutOfSsaProgram`, build a reconstructable interference graph over retained `SsaValue` identities and out-of-SSA temporaries, then produce a deterministic assignment under a caller-supplied physical-register budget with an explicit spill set for unassigned locations. Verify liveness/interference against an independent forward path-to-use oracle and verify every simultaneously live assigned pair receives different registers. Keep optimal coloring/minimum-spill claims, copy coalescing, spill-code insertion/rewrite, MemorySSA, optimization passes, and incremental CFG maintenance outside this first allocation slice.

## Phase 49 — interference-safe copy coalescing — SEALED

- [x] Build deterministic move-preference/coalescing classes over the sealed phi-free program and Phase-48 interference graph, merging only locations that remain interference-safe in the quotient graph; recolor the quotient under the caller-supplied register budget, map assignments/spills back to original locations, and expose scheduled copies made redundant by coalescing. Verify that no original interfering pair collapses into one class, every retained assigned quotient interference edge remains register-safe, and redundant-copy witnesses are replayable. Keep maximum-coalescing, minimum-register/minimum-spill, spill-code insertion/rewrite, machine opcode/memory-addressing semantics, MemorySSA, and backend-wide optimality claims outside this slice.

## Phase 50 — explicit backend storage and spill materialization — SEALED

- [x] Lower the sealed Phase-49 coalesced allocation into a machine-independent backend storage IR with physical-register bindings for assigned classes, one deterministic stack slot per spilled coalescing class, and an explicit abstract spill-scratch-register namespace. Rewrite entry/exit scheduled copies and phi-free instruction uses/definitions into replayable register moves plus explicit stack reload/store operations, omitting only copies whose final backend storage is identical. Preserve CFG/block/operation order, expose complete virtual-location-to-storage provenance and maximum scratch-register demand, and verify structural replay against the original phi-free program. Keep concrete ISA opcodes/addressing modes, byte-level frame layout/alignment, calling conventions, hardware scratch-register reservation, spill-cost optimization, post-rewrite reallocation, MemorySSA, and backend-wide optimality outside this baseline.

## Phase 51 — scratch-aware backend register reservation — ACTIVE FRONTIER

- [ ] Given a finite physical-register file, deterministically search reserved spill-scratch capacity while rerunning sealed Phase-49 coalescing and Phase-50 spill materialization under the reduced allocatable budget. Select the smallest reservation whose measured abstract scratch demand fits the reserved suffix, map every used abstract scratch index to a non-overlapping real physical register, expose reservation/feasibility provenance, and verify that assigned registers and scratch registers are disjoint on every backend operation. Keep byte-level frame layout/alignment, concrete ISA instruction selection/addressing, calling conventions, minimum-spill/global-register optimality, MemorySSA, and post-rewrite allocation optimization outside this slice.
