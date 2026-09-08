# Phase 28 — Bounded Affine-Gap BWT Search

## Scope

Phase 28 extends the sealed Phase-27 unit-cost Levenshtein state machine with an explicit affine gap score. A contiguous insertion or deletion gap of length `L >= 1` costs `gap_open + (L-1) * gap_extend`; substitutions have an independent positive cost. Exact byte matches cost zero.

The result remains a set of exact substring start positions. A start position is returned iff at least one substring beginning there has affine-gap score at most the requested budget. The implementation is a first-principles bounded search, not a seed-and-extend or polynomial-time approximate-index algorithm.

## Scoring-state invariant

The next cost of an insertion/deletion transition depends on whether the immediately preceding edit remains in the same gap run. Every search node therefore carries one of three gap modes: none, insertion, or deletion. The domination key includes that mode together with pattern progress, candidate length, and paired forward/reverse BWT intervals. Collapsing states across gap modes would be unsound because their next transition costs differ.

An exact match or substitution resets the mode to none. Continuing the current insertion/deletion run pays `gap_extend`; opening that gap from any other mode pays `gap_open`. Direct insertion-to-deletion or deletion-to-insertion adjacency is permitted by this explicit scoring contract and opens a new gap run.

All substitution, gap-open, and gap-extend costs must be positive. This keeps bounded search finite and makes Dijkstra ordering well-founded.

## Search order and BWT integration

Arbitrary positive edge costs replace Phase-27's 0/1 deque with the repository's first-principles `BinaryHeap` as a Dijkstra frontier. Queue ties are broken by a checked monotonic serial number, making replay deterministic.

Search still proceeds right-to-left. Every text-consuming transition calls the sealed `BidirectionalBwtByteIndex::extend_left`; empty exact intervals prune immediately. Non-empty terminal candidate strings are resolved through zero-budget sealed BWT/Hamming locate, never by scanning the source text.

If the pattern is empty, or deleting the complete pattern as one affine deletion run fits the budget, the empty substring is already a witness at every boundary. The implementation returns all `n+1` boundaries without expanding the search frontier.

## Diagnostics

The result exposes expanded states, semantic transitions considered, empty-interval prunes, over-budget prunes, dominated states, terminal states, and peak frontier size. Counters reject overflow. Repeating the same immutable query must reproduce both positions and diagnostics.

## Complexity boundary

The reachable search-state count may grow exponentially with score budget and byte-alphabet branching. Each text-consuming branch inherits sealed bidirectional BWT extension cost. Frontier operations add binary-heap logarithmic overhead; domination uses an ordered map; every distinct terminal candidate additionally pays exact BWT locate; output union uses `O(n)` query-local boundary state.

No seed-and-extend, bit-parallel, probabilistic, polynomial-time approximate-index, affine-alignment acceleration, r-index-optimal, or universal performance claim is made.

## Verification

The primary oracle is independent affine-gap dynamic programming with separate match, insertion-gap, and deletion-gap matrices. Tests enumerate every substring length at every start boundary and compare exact returned positions. The oracle never calls BWT, suffix-array, Phase-27 search, or production frontier logic.

Secondary cross-phase integration verifies that `substitution = gap_open = gap_extend = 1` produces exactly the sealed Phase-27 unit-cost edit-distance positions.

Deterministic cases cover insertion/deletion gap runs, explicit gap switching, empty/full-deletion boundary semantics, arbitrary `0x00`/`0xFF` bytes, invalid zero costs, checked `size_t` score boundaries, and diagnostics replay. Fixed-seed tiny randomized cases exercise independent DP differential verification.

## Status

Implementation candidate prepared. Phase 28 remains active until exact remote full-repository CI, clean integration, merged-main CI, and an independent sealing audit all succeed.
