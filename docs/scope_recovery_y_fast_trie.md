# Scope recovery — bucketed y-fast predecessor set

## Coverage decision

Fresh live audit at `main@b4971cde8067dedd3712e4c2ba5104a618063f1c` found no y-fast implementation in default-branch code, pull-request history, or branch namespace. The sealed bounded-universe van Emde Boas checkpoint (PR #177) explicitly left y-fast `O(n)`-space behavior unclaimed, and the later x-fast trie checkpoint (PR #289) again documented y-fast space/update bounds as out of scope. This slice closes that recorded gap without touching the frozen compiler/backend roadmap or the recent closure/path-cover/matching surface.

## Production contract

`YFastTrieSet(universe_bits, seed)` stores unique unsigned keys in `[0,2^w)` for `1 <= w <= 64` and provides:

- exact insert / erase / contains;
- minimum / maximum;
- strict predecessor / successor;
- full `uint64_t` endpoint support at width 64;
- deterministic replay for a fixed seed and successful-insertion trace;
- ordered value / representative / bucket-size / bucket-root diagnostics;
- an expensive `valid_structure()` replay of representative, bucket-range, randomized-treap, and global-order invariants.

The implementation reuses the sealed `XFastTrieSet` only for bucket boundaries. Keys live exactly once in randomized treap buckets. The permanent zero boundary selects the first bucket even when zero itself is absent. Additional boundaries are numeric separators created by bucket splits; they need not remain member keys after later deletion.

## Bucket and representative invariants

Let `B = max(2,w)`.

- With more than one bucket, every bucket contains between `B` and `4B` keys.
- A sole bucket contains at most `4B` keys.
- A bucket boundary is at most every key in its bucket and strictly below every key in the next bucket.
- Buckets are disjoint and their in-order concatenation is the complete global set.
- A split occurs only after size `4B+1`, producing two buckets near size `2B`.
- Deletion underflow occurs below `B`; the bucket is merged with a neighbor, and a combined bucket above `4B` is immediately repartitioned near the middle.

The `B` versus `4B` hysteresis is deliberate. It prevents an insert/erase pair near one threshold from forcing an expensive boundary rebuild on every operation.

Each bucket is a first-principles treap. Priorities are raw `std::mt19937_64` outputs consumed only by successful new-key insertions; split/merge preserves existing priorities. Same seed plus the same successful-insertion/erasure trace therefore replays the same bucket-root witness. Priority ties use key order only as a deterministic total-order fallback.

## Complexity boundary

For universe size `U=2^w`, the representative x-fast trie performs expected hash-dictionary prefix probes and the randomized bucket treap has expected logarithmic height in bucket size.

- predecessor / successor / contains: expected `O(log w) = O(log log U)`;
- minimum / maximum: representative/bucket extrema plus hash lookup, expected `O(1)` at the abstract dictionary level;
- ordinary bucket insert / erase: expected `O(log w)`;
- split/merge rebuild: `O(w log w)` expected direct work plus an `O(w)` representative-index update, but hysteresis spaces such rebuilds by `Omega(w)` successful local changes, giving expected-amortized `O(log w)` update work;
- key storage is `O(n)`; with more than one bucket, `r*w <= n` for `r` representatives, so x-fast prefix payload is also `O(n)`. The concrete C++ object additionally owns `O(w)` level-container overhead, so the honest resident bound is `O(n+w)` rather than pretending that fixed implementation scaffolding disappears for tiny `n`.

These are expected/amortized bounds under the repository's ordinary `std::unordered_map` model and randomized treap priorities. An individual treap can be linear-height on an unlucky priority realization, and adversarial hash-table worst cases are not excluded. No deterministic `O(log log U)` bound, cryptographic randomness, high-probability numeric tail bound, or allocation-failure strong guarantee is claimed.

## Verification

Focused pre-upload logic verification passed:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak checking: 4/4.

The focused harness used an API-compatible representative-index test double because the local container cannot reach GitHub; this pull request's full-repository CI is the authoritative integration gate against the real sealed `XFastTrieSet`.

Evidence includes invalid universe widths and keys, empty/full-width boundaries, duplicate insertion, absent erase, repeated split/merge and representative-count space invariants, same-seed bucket-root replay, 40,000 fixed-seed mixed operations in a 16-bit universe against independent `std::set<uint64_t>`, and 15,000 additional full-width update operations with periodic predecessor/successor/structure checks.

The `std::set` oracle is test-only and is not used by production. `valid_structure()` is intentionally expensive and is sampled periodically during long randomized traces rather than misrepresented as constant-time production work.

## Scope / non-claims

This recovery changes exactly four paths: one header-only implementation, one repo-native test header, one test-registry include, and this proof document. It does not modify CMake, README, the frozen historical ROADMAP, recovery authority, workflows, benchmarks, or compiler/backend code.

This implementation is a first-principles bucketed y-fast predecessor structure that closes the recorded x-fast space gap. It does not claim a stronger deterministic hash bound, persistent/concurrent updates, multiset semantics, implicit arbitrary-width integers, or a universal performance advantage over the sealed vEB/x-fast baselines.
