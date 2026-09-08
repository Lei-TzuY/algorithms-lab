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

- [x] Exact substring start-position search under a bounded byte-oriented Levenshtein budget, extending the sealed BWT state machine with substitution, insertion, and deletion transitions. Production retains deterministic replayable state/branch diagnostics, deduplicates dominated equivalent search states without direct scanning, reconstructs exact start-position witnesses through the sealed BWT locate machinery, and makes the exponential-in-budget baseline cost explicit. Verification compares against an independent direct dynamic-programming substring oracle. No seed-and-extend, affine-gap, probabilistic, or polynomial-time approximate-index claim is implied.

## Phase 28 — bounded affine-gap approximate BWT search — SEALED

- [x] Exact substring start-position search under a bounded byte-oriented affine-gap score, extending the sealed Phase-27 state machine with explicit gap-open versus gap-extend memory for insertion and deletion runs. Production preserves deterministic replayable diagnostics, exact BWT-only witness reconstruction, domination keyed on the full scoring state, and explicit exponential baseline cost. Verification compares against an independent affine-gap dynamic-programming substring oracle, with `{1,1,1}` scoring cross-checked against sealed Phase 27. No seed-and-extend, probabilistic, polynomial-time approximate-index, or asymptotically optimal compressed-search claim is implied.

## Phase 29 — BWT text reconstruction and bounded extraction substrate — SEALED

- [x] Reconstruct exact source bytes from the conceptual-sentinel BWT/LF state without retaining a separate source-text copy, then expose validated half-open bounded extraction over arbitrary bytes with deterministic LF-step diagnostics. Verification compares full reconstruction and randomized slices against the constructor input while keeping resident state on the sealed BWT representation. The sealed baseline intentionally uses linear reconstruction/query workspace and exposes that cost; it is a correctness substrate rather than a compressed-optimal extraction or universal performance claim.

## Phase 30 — exact seed-and-verify bounded edit-distance search — SEALED

- [x] Generate complete candidate start positions from `k+1` non-overlapping exact pattern seeds located through the sealed BWT index, account explicitly for the `±k` start displacement induced by preceding insertions/deletions, reconstruct the source text once through the sealed Phase-29 LF substrate, and verify each unique candidate with exact bounded Levenshtein dynamic programming. Verification compares against an independent direct-DP substring oracle and cross-checks bounded cases with the sealed Phase-27 state-expansion search. The implementation exposes seed-hit/candidate/reconstruction/verification diagnostics and makes its candidate-sensitive polynomial verification cost explicit; no heuristic seed, universal speedup, compressed-optimal extraction, or hidden exactness assumption is claimed.

## Phase 31 — periodic-sample local BWT extraction — ACTIVE FRONTIER

- [ ] Add a suffix-position-to-row inverse over the sealed Phase-20 periodic locate samples, then expose exact half-open local text extraction that starts from the nearest sampled suffix position at or after the requested end and LF-walks backward only far enough to fill the requested range. Preserve arbitrary-byte/conceptual-sentinel semantics, expose LF-step and resident inverse-sample payload diagnostics, verify randomized slices against direct source substrings across varied sample rates, and make the explicit `O((end-begin)+sample_rate)` LF-step / `O(end-begin)` output-space tradeoff executable. This is a bounded local-extraction improvement, not a compressed-optimal random-access or universal speedup claim.
