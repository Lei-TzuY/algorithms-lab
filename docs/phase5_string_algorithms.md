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

## Scope boundary and frontier

This slice deliberately exposes the Z-array rather than building pattern matching through `pattern + sentinel + text`: arbitrary-byte inputs have no universally safe one-byte sentinel. KMP already owns exact all-occurrence matching, while Z contributes a distinct reusable prefix-LCP state model.

With KMP and Z-function represented, the next ordered Phase-5 frontier is rolling hash, where collision semantics and non-cryptographic guarantees must be explicit before suffix-array structures.
