# Phase 32 — candidate-local exact seed-and-verify

Phase 32 keeps the sealed Phase-30 exact `k+1` seed completeness argument but
replaces unconditional full-text reconstruction during verification with the
sealed Phase-31 periodic-sample local extraction substrate.

## Contract

`locate_bwt_candidate_local_seed_verify_edit_distance` returns exactly the
substring start positions whose byte-oriented Levenshtein distance is at most
`k`. Empty patterns and `k >= pattern.size()` retain the repository convention
that every boundary `0..n` matches.

For the non-trivial case, the pattern is partitioned into `k+1` non-empty,
non-overlapping exact seeds exactly as in Phase 30. Every true match within edit
distance `k` contains at least one unchanged seed. A seed occurrence plus the
`[-k,+k]` start displacement caused by preceding insertions/deletions therefore
produces a superset of all valid starts.

Only marked starts with enough remaining text are verified. A verification
window is the half-open source range
`[start, min(n, start + pattern.size() + k))`, with overflow-safe saturation to
the remaining source length. The sealed `BwtPeriodicSampleTextExtractor`
reconstructs that range from the nearest periodic suffix-position sample. The
same bounded Levenshtein DP semantics used by Phase 30 then verify the candidate
at local offset zero.

## Diagnostics

The result exposes:

- seed count and exact seed-occurrence count;
- unique and actually verified candidate counts;
- exact DP-cell count;
- number of local extraction windows;
- total extracted source bytes;
- total LF transitions spent on local extraction.

Each verified candidate uses exactly one extraction window in this baseline.
For sample rate `s`, every non-empty window of length `w` performs between `w`
and `w+s-1` LF transitions, inheriting the Phase-31 extractor bound.

## Verification

The test suite compares every Phase-32 result against both:

1. the sealed Phase-30 full-reconstruction seed-and-verify implementation; and
2. an independent direct substring Levenshtein-DP oracle.

Randomized arbitrary-byte cases vary text length, pattern length, edit budget,
and locate sample rate. Deterministic cases cover substitutions, insertions,
deletions, negative seed-center displacement, embedded NUL/high-bit bytes,
empty/large-budget fast paths, and absent seeds.

A sparse-candidate regression uses a 4096-byte text with one exact seed hit and
verifies that measured local-extraction LF work is strictly below Phase 30's
full 4096-step reconstruction. This is evidence for that workload, not a
universal speedup claim.

## Complexity boundary

Let `C` be the number of verified candidates, `m` the pattern length, `k` the
edit budget, and `s` the locate sample rate. Candidate generation retains the
Phase-30 seed-search/candidate-marking cost. Verification extracts at most
`m+k` bytes per candidate and performs at most `m+k+s-1` LF transitions per
candidate, followed by `O(m(m+k))` DP work. In the worst case `C` may be
`Theta(n)`, so this phase does not claim a sublinear worst-case query or a
universal improvement over full reconstruction.

The implemented gain is candidate-sensitive: sparse candidate sets avoid the
unconditional `Theta(n)` source reconstruction performed by Phase 30.
