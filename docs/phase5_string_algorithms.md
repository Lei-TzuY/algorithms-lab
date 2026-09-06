# Phase 5 — string algorithms

Phase 5 begins with exact byte-string pattern matching. The first slice is Knuth-Morris-Pratt (KMP) because it makes reusable prefix/failure state explicit and demonstrates how a matcher avoids restarting comparisons after a mismatch.

## KMP prefix / failure function

For a pattern `P`, define `pi[i]` as the length of the longest **proper** prefix of `P[0,i+1)` that is also a suffix of that same prefix. `pi[0] = 0`.

When processing `P[i]`, let `matched = pi[i-1]`. If `P[i]` does not extend that border, the next possible border is the longest border of the current border, `pi[matched-1]`. Repeating this fallback cannot skip a valid longer candidate: every proper border of a string's border is also a border of the string. Once bytes match, the border extends by one.

The implementation stores the entire prefix table because the table is both an executable result and the failure state reused by matching.

## All-occurrence search

While scanning text left to right, `matched` is the length of the pattern prefix matching the suffix of text processed so far. A mismatch follows the same prefix-table fallback chain rather than moving the text index backward. A full pattern match records its starting offset and then falls back to `pi[m-1]`, which preserves a suffix that may begin an overlapping next match.

The API is byte-oriented over `std::string_view`; embedded nulls and high-bit bytes have no special meaning. An empty pattern matches every text boundary, so `kmp_find_all(text, "")` returns offsets `0..text.size()`. This makes the empty case explicit instead of relying on an implementation accident.

### KMP complexity and verification

Prefix preprocessing is `O(m)` for pattern length `m`. Searching is `O(n + m)` including preprocessing for text length `n`: each successful comparison advances the text scan, and failure transitions strictly shorten `matched`, so fallback work is amortized linear. Storage is `O(m + z)` where `z` is the number of returned matches.

Deterministic tests cover known prefix tables, empty text/pattern combinations, no/full matches, heavily overlapping matches, embedded null bytes, and `0x80`/`0xFF` bytes. Fixed-seed randomized byte strings compare the production prefix table against an independent quadratic border scan and compare every returned match position against an independent naïve substring scan.

## Z-function — rightmost prefix-match box

For an input `S` of length `n`, `z[i]` is the length of the longest common prefix of `S` and `S[i,n)`. This repository defines `z[0] = n` for non-empty input; the empty input produces an empty vector. The API is byte-oriented, so embedded nulls and high-bit bytes are ordinary data.

The linear algorithm maintains the rightmost known prefix-match interval as a half-open box `[left,right)`: `S[left,right)` equals `S[0,right-left)`. When the next index `i` lies inside the box, `z[i-left]` describes the corresponding prefix-aligned match already seen. The implementation can therefore seed `z[i]` with `min(right-i, z[i-left])` without comparing those bytes again, then extend only beyond the known box. If the resulting match ends farther right, `[left,right)` is replaced by `[i,i+z[i])`.

**Z-box invariant.** Before processing index `i`, `[left,right)` is the rightmost prefix-match interval discovered among earlier positions, and every reused byte lies entirely inside that already-verified interval. Direct extension compares only bytes after the reusable prefix. This differs from KMP's border/failure-chain state: KMP reuses nested suffix-prefix borders while scanning a separate text, whereas the Z-function reuses a spatial prefix-match interval within one string.

### Z complexity and verification

The function runs in `O(n)` time and returns `O(n)` state, with `O(1)` auxiliary state beyond the output vector. Every direct comparison that extends `right` advances the global right boundary, while reuse inside the box performs no byte comparison.

Deterministic tests cover empty/singleton inputs, all-equal bytes, no repeated prefix, periodic overlap, embedded null bytes, and high-bit bytes. Seven hundred fixed-seed randomized byte strings are compared element-for-element against an independent naïve longest-common-prefix scan. Additional invariants verify `z[i] <= n-i`, exact equality for every claimed prefix byte, and a mismatch immediately after each non-maximal match.

## Rolling hash — immutable substring fingerprints

`RollingHash` snapshots an arbitrary byte string and precomputes two polynomial prefix tables plus powers of a fixed base. Each byte is converted through `unsigned char` and encoded as `1..256`, so null and high-bit bytes remain ordinary deterministic symbols.

For modulus `M`, the prefix recurrence is `H[i+1] = (H[i] * B + symbol[i]) mod M`. A substring `[l,r)` of length `d` therefore has normalized fingerprint `H[r] - H[l] * B^d (mod M)`, independent of its original position. Precomputed powers make extraction `O(1)` after `O(n)` construction.

The implementation uses fixed base `257` and the two distinct prime moduli `1,000,000,007` and `1,000,000,009`. With these fixed parameters, every intermediate multiplication is safely representable in `uint64_t`; modular reduction is explicit rather than relying on unsigned overflow behavior.

### Collision and security semantics

This is a deterministic **non-cryptographic** fingerprint. Two byte-identical substrings necessarily receive the same pair, but equal fingerprints do **not** prove byte equality: modular polynomial hashes can collide, and fixed public parameters are not appropriate for adversarial collision resistance. Callers that require exact equality must confirm candidate matches by comparing bytes or use a deterministic exact-string algorithm.

Two modular components avoid depending on one residue alone, but this repository makes no numeric collision-probability claim without a specified input distribution or adversary model. The construction is not a cryptographic hash and does not remove the possibility of collisions.

### Rolling-hash complexity and verification

Construction is `O(n)` time and storage. `fingerprint(begin,end)` is `O(1)` and uses half-open ranges with strict bounds validation. The object owns only precomputed numeric state, so later mutation of the caller's source string cannot change the snapshot fingerprint index.

Deterministic tests cover empty/range behavior, equal substrings at different positions, snapshot immutability, embedded nulls, and high-bit bytes. Five hundred fixed-seed arbitrary-byte strings execute eighty random substring queries each; every production fingerprint is compared against an independent direct polynomial evaluation over that substring, which does not use prefix extraction.

## Scope boundary and frontier

KMP supplies deterministic exact matching, Z-function supplies deterministic prefix-LCP reuse, and rolling hash supplies constant-time collision-prone substring fingerprints after preprocessing. Their verification oracles remain structurally independent.

The next ordered Phase-5 frontier is suffix array and related deterministic structures. Rolling-hash equality must never be promoted into an exact-match or cryptographic guarantee in later integration.
