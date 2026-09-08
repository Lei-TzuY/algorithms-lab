# Phase 22 sealing audit

## Exact integrated checkpoint

Phase 22 is audited against merged `main` commit `15d081235c1176e82dd3efb0cbd17b978cf0127d` (`Phase 22: run-aware BWT toehold locating`). GitHub Actions run `34175758276` completed successfully on all repository gates:

- GCC release;
- Clang release;
- GCC ASan+UBSan.

There were no open pull requests or open issues at the audit point.

## Capability actually added

The sealed capability is one exact occurrence witness carried through FM backward search from `O(R)` resident run-boundary suffix-position samples. It is not an alternate spelling of Phase-20 periodic locate sampling:

- `RunLengthByteRankIndex::select(byte, ordinal)` reuses the Phase-21 run metadata rather than materializing a full occurrence table;
- suffix positions are sampled at starts/ends of byte runs in the **full conceptual BWT**, so the conceptual sentinel remains a real run break even when deleting it makes equal byte runs appear adjacent;
- `locate_one_toehold()` keeps an exact `(row, SA[row])` witness inside every non-empty search interval;
- a matching toehold byte advances directly by LF; otherwise the nearest matching byte on one side is forced to be a run boundary, so its stored suffix-position sample can re-establish the invariant;
- missing samples, impossible side relations, zero-position byte rows, or a witness escaping the next interval fail as logic errors rather than silently consulting a full suffix-row table.

The source audit confirmed that the production path implements these checks directly. The existing full `locate()` remains a separate Phase-20 periodic-sample path.

## Correctness and evidence boundary

Deterministic tests cover empty/absent patterns, overlaps, arbitrary bytes, periodic-sample-rate independence, and the conceptual-sentinel split adversary `{0,1,1}` whose full BWT contains two `1` runs separated by the sentinel.

The integrated suite also checks 500 fixed-seed random texts of length 0..80 with 80 queries each against independent direct substring scanning. A returned toehold must be one of the direct-scan occurrences, and absence must agree exactly. Run-sample cardinality and logical payload identities are verified independently from query correctness.

These tests are implementation evidence, not the proof of the storage or toehold theorem. The resident sample bound `S <= 2(R+1)` follows from sampling start/end of each full-conceptual-BWT byte run, while the fallback correctness follows from the nearest-occurrence/run-boundary argument.

## Complexity and claim audit

The phase keeps two `size_t` vectors for `S = O(R)` samples. With Phase-21 rank/access and symbol-select plus binary-searched boundary samples, `locate_one_toehold(pattern)` is conservatively `O(m log R)` for pattern length `m`.

No claim is made that construction is compressed: suffix-array construction and temporary `O(n)` row-position workspace remain. No claim is made that this is a full r-index or that it supports r-index-optimal locate-all bounds.

## Promotion decision

Phase 22 is sealed rather than extended with wrappers around `locate_one_toehold`.

A coherent next frontier exists: resolve **every** row in an exact backward-search interval using only Phase-22 run-boundary suffix samples plus the conceptual sentinel as an implicit sample. That changes the full-locate resident-state dependency: Phase-20 periodic suffix samples are no longer consulted by the new path. The tradeoff is intentionally slower worst-case LF walking per returned occurrence.

Phase 23 therefore targets run-boundary-sampled full occurrence enumeration with complete direct-scan differential verification and an explicit non-r-index-optimal complexity boundary.
