# Phase 15 — online paging and competitive analysis

Phase 15 introduces a correctness model that is different from exact, amortized, randomized, approximation-ratio, or persistent-state reasoning: an online algorithm must commit before future requests are known, while its quality is compared with an offline optimum that sees the entire request sequence.

## Public surface

`lru_paging(requests, capacity)` is the online policy. It returns the cache capacity, total fault count, and one `PagingEvent` per request. Each event records the requested signed-64-bit page identifier, whether the request hit, and the evicted page on a full-cache fault. The trace is therefore replayable rather than exposing only a scalar fault count.

`belady_optimal_paging(requests, capacity)` is an offline comparator, not an online policy. It uses future requests and evicts a resident page whose next request lies farthest in the future. Pages never used again are preferred victims; when several such pages exist, the numerically smallest page is evicted to make the witness deterministic.

Capacity zero is explicitly supported: every request faults and no page is stored or evicted.

## LRU online invariant

The LRU implementation stores resident pages in most-recently-used to least-recently-used order. A hit moves exactly that page to the front. A miss inserts the requested page at the front; if the cache was full, the unique page at the back is evicted first. Decisions depend only on the request prefix seen so far.

The implementation uses an ordered map from page ID to recency-list iterator. This deliberately gives a deterministic worst-case `O(log k)` membership/update bound instead of hiding hash-table assumptions. For `n` requests and cache capacity `k`, LRU takes `O(n log k)` time (with the capacity-zero case handled directly), `O(k)` working state, and `O(n)` output trace storage.

## Offline optimum and independent verification

Belady's furthest-in-future rule is the classical offline optimum for paging. The production comparator tracks future occurrence positions and scans the current cache on a fault. Its direct implementation is intentionally simple rather than optimized because it exists to expose the online/offline boundary.

Tests do not accept Belady's implementation as its own oracle. For bounded request sequences an independent dynamic program enumerates all reachable cache-content states and minimizes future faults over every possible eviction. Belady's fault count must equal this exact DP optimum.

## Competitive boundary

For capacity `k >= 1`, the documented universal claim is the standard LRU competitive bound

`faults_LRU(sequence) <= k * faults_OPT(sequence) + k`.

A phase argument gives the proof obligation: partition the sequence into maximal phases containing at most `k` distinct pages. LRU faults at most `k` times in each phase. Between consecutive phases, any cache of size `k` must fault at least once, so if there are `p` phases then `OPT >= p - 1` and `LRU <= k p <= k * OPT + k`.

The additive term is kept explicit; the repository does not strengthen the theorem merely because finite test corpora look better. Capacity zero is outside this competitive-ratio statement and has its own exact semantics.

## Verification

Deterministic tests cover empty input, capacity zero, full signed-64-bit page IDs, exact LRU hit/eviction behavior, repeated-run determinism, and Belady's tie rule for pages that are never used again.

For every sequence over a three-page alphabet of length zero through six, capacities one and two are checked against the independent exact offline DP. An additional 800 fixed-seed random sequences (length at most eleven, capacities zero through four) verify:

- both returned traces can be replayed without violating capacity;
- an independent recency model agrees with every LRU hit and eviction;
- Belady's fault count equals the exact offline DP optimum;
- LRU never beats the offline optimum;
- for positive capacity the bounded corpus satisfies the theorem-derived `k * OPT + k` inequality.

These tests are executable evidence, not a proof of the universal competitive theorem; the phase argument above is the proof obligation.

## Frontier

This slice is intended to establish the Phase-15 hypothesis: online decisions, offline comparison, replayable action traces, and an honest competitive-analysis boundary. A later architecture audit should seal the phase unless another online problem introduces a genuinely different guarantee mechanism; adding replacement-policy variants merely to increase algorithm count is not sufficient.
