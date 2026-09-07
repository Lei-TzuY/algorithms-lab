# Phase 15 sealing audit

Phase 15 is sealed after the online paging implementation reached merged `main` as `f59e4073d0afa015f6593ca123c1e068edb9e40d` and exact merged-main CI run `34166001874` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

## Capability boundary

The phase establishes online-vs-offline reasoning as an executable contract rather than merely adding a cache replacement routine:

- `lru_paging` makes decisions from the request prefix only;
- each request yields a replayable hit/fault/eviction event instead of only a scalar score;
- `belady_optimal_paging` is explicitly separated as a future-aware offline comparator;
- an independent cache-state dynamic program computes the exact bounded offline optimum rather than accepting Belady as its own oracle;
- the universal claim `LRU <= k * OPT + k` is documented as a theorem-derived competitive boundary, with the phase proof kept distinct from finite test evidence;
- capacity zero and full signed-64-bit page identifiers have explicit deterministic semantics.

Exhaustive bounded sequences and 800 fixed-seed random traces replay both policies, independently reconstruct LRU recency, require Belady to equal the exact DP optimum, and check the competitive inequality on the bounded corpus. These checks are evidence for the implementation; they are not presented as a proof of the universal competitive theorem.

## Integration evidence

The exact candidate and merged-main repository matrices both passed GCC, Clang, and sanitizer builds/tests. No correctness, determinism, or integration blocker remains. The ordered-map implementation intentionally keeps deterministic worst-case membership/update behavior instead of hiding hash-table assumptions.

## Why the phase stops here

The educational hypothesis is the reasoning model: an online policy commits without future knowledge, an offline optimum supplies the comparison baseline, and quality is characterized by a sequence-wise competitive guarantee. Adding FIFO, random replacement, or more cache-policy names would mainly broaden the catalog without introducing another guarantee mechanism. Those variants remain out of scope unless a future architecture needs them.

## Promotion

Phase 16 moves to streaming and sublinear state. The first hypothesis is Misra-Gries heavy hitters: one-pass processing with at most `k-1` counters, an executable cancellation-round witness, deterministic one-sided frequency error `f(x)-c(x) <= D <= floor(n/k)`, and state whose size is independent of stream length. This adds a memory/error contract that is distinct from Phase-15 competitive analysis and Phase-11 probabilistic execution.
