# Phase 32 Sealing Audit

## Decision

Phase 32 is sealed. The candidate-local exact seed-and-verify implementation is merged on `main@98b7765c316ee5d67add292edcabae5cb83b897b`, and the exact merged-main CI run `34197072072` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. The remaining unchecked ROADMAP entry was status debt by explicit design of PR #81, not missing production behavior.

## Executable capability

Phase 32 preserves the sealed Phase-30 `k+1` exact-seed candidate-completeness argument and replaces unconditional whole-text reconstruction during verification with bounded Phase-31 periodic-sample extraction windows. Every actually verified candidate extracts `[start, min(n, start + m + k))`, then applies the same exact bounded byte-oriented Levenshtein semantics at local offset zero.

Production exposes seed count, seed-occurrence count, unique/verified candidate counts, DP-cell count, extraction-window count, extracted bytes, and LF-step count. Empty-pattern and `k >= pattern.size()` repository boundary semantics are preserved.

## Correctness and integration evidence

PR #81 exercises deterministic substitution/insertion/deletion cases, negative seed displacement, embedded NUL/high bytes, empty/large-budget fast paths, absent seeds, and varied periodic-sample rates. Its fixed-seed randomized suite checks every result three ways: Phase 32 equals the sealed Phase-30 full-reconstruction seed-and-verify implementation and equals an independent direct substring-DP oracle.

The sparse-candidate regression demonstrates one workload where local extraction performs strictly less LF work than the Phase-30 full reconstruction baseline. That measurement remains workload evidence only; it is not promoted into a universal performance claim.

The merged-main CI matrix verifies the complete repository after integration, not merely the focused Phase-32 tests.

## Cost and claim boundary

For `C` verified candidates, pattern length `m`, edit budget `k`, and locate sample rate `s`, Phase-32 verification extracts at most `m+k` bytes and at most `m+k+s-1` LF transitions per candidate, followed by `O(m(m+k))` DP work per candidate. Candidate generation retains the sealed Phase-30 cost.

`C` can still be `Theta(n)`. Dense candidates or large sample rates can equal or exceed full-reconstruction work, so Phase 32 does not claim sublinear worst-case query time, universal speedup, compressed-optimal extraction, or r-index-optimal approximate matching.

## Architecture audit

No unresolved correctness, integration, or representability blocker was found. Phase 32 closes the Phase-31 promotion target cleanly and further per-candidate extraction variants inside this phase would now be low-value surface farming.

The remaining coherent gap is at a different granularity: overlapping candidate verification windows are extracted independently. Dense or clustered candidates can therefore reconstruct the same source bytes and nearby LF paths repeatedly even though their exact verification windows overlap substantially.

## Promotion

Phase 33 promotes to coalesced candidate-window exact verification. It must preserve the exact Phase-32 candidate set and bounded Levenshtein semantics, sort and merge overlapping/touching verification intervals, extract each merged interval once through the sealed Phase-31 extractor, and verify each candidate from its offset in the shared buffer.

The new slice must expose original-candidate versus merged-window counts plus extracted-byte/LF-step diagnostics, compare exact positions against sealed Phase 32 and an independent direct-DP oracle, cover arbitrary bytes and varied sample rates, and include a clustered-candidate regression where measured merged-window work is below the Phase-32 per-candidate baseline. This is a batching architecture change, not a universal speedup claim.
