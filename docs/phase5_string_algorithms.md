# Phase 5 — string algorithms

Phase 5 begins with exact byte-string pattern matching. The first slice is Knuth-Morris-Pratt (KMP) because it makes reusable prefix/failure state explicit and demonstrates how a matcher avoids restarting comparisons after a mismatch.

## KMP prefix / failure function

For a pattern `P`, define `pi[i]` as the length of the longest **proper** prefix of `P[0,i+1)` that is also a suffix of that same prefix. `pi[0] = 0`.

When processing `P[i]`, let `matched = pi[i-1]`. If `P[i]` does not extend that border, the next possible border is the longest border of the current border, `pi[matched-1]`. Repeating this fallback cannot skip a valid longer candidate: every proper border of a string's border is also a border of the string. Once bytes match, the border extends by one.

The implementation stores the entire prefix table because the table is both an executable result and the failure state reused by matching.

## All-occurrence search

While scanning text left to right, `matched` is the length of the pattern prefix matching the suffix of text processed so far. A mismatch follows the same prefix-table fallback chain rather than moving the text index backward. A full pattern match records its starting offset and then falls back to `pi[m-1]`, which preserves a suffix that may begin an overlapping next match.

The API is byte-oriented over `std::string_view`; embedded nulls and high-bit bytes have no special meaning. An empty pattern matches every text boundary, so `kmp_find_all(text, "")` returns offsets `0..text.size()`. This makes the empty case explicit instead of relying on an implementation accident.

### Complexity

Prefix preprocessing is `O(m)` for pattern length `m`. Searching is `O(n + m)` including preprocessing for text length `n`: each successful comparison advances the text scan, and failure transitions strictly shorten `matched`, so fallback work is amortized linear. Storage is `O(m + z)` where `z` is the number of returned matches.

### Verification

Deterministic tests cover known prefix tables, empty text/pattern combinations, no/full matches, heavily overlapping matches, embedded null bytes, and `0x80`/`0xFF` bytes.

Fixed-seed randomized byte strings compare the production prefix table against an independent quadratic border scan and compare every returned match position against an independent naïve substring scan. Returned positions are additionally checked for strict ordering, bounds, and actual byte equality.

## Frontier

KMP is the first Phase-5 slice. Z-function is next so the repository can compare two different linear-time prefix-reuse formulations before moving to rolling hash and suffix-array structures.
