# Scope recovery: Lyndon factorization via Duval

## Coverage decision

This slice continues the post-Phase-69 algorithms/data-structures scope recovery with a classical combinatorics-on-words proof model that is absent from live code, branch names, and PR history: Chen-Fox-Lyndon factorization computed by Duval's linear scan.

Exact base is `main@74e5f877fddc502b0d6d340097b79ff1266c5821`, where FIFO push-relabel maximum flow is merged-main green in CI run `34726877204` on GCC release, Clang release, and GCC ASan+UBSan. The repository had zero open PRs and zero open issues before branch creation. Fresh searches found no Lyndon or Duval implementation or branch. The genuinely occupied long-lived `scope-recovery-minimum-cycle-basis` surface is deliberately untouched. Historical compiler/backend ROADMAP headings remain non-authoritative under `scope_recovery_after_phase69.md`.

## Production contract

`duval_lyndon_factorization(text)` returns a vector of contiguous half-open byte ranges whose concatenation is the input string.

- empty input returns an empty factor sequence;
- every non-empty factor is a Lyndon word;
- factors are lexicographically non-increasing;
- repeated equal factors are preserved separately;
- comparison order is explicit unsigned byte order `0..255`, so results do not depend on whether host `char` is signed;
- arbitrary bytes, including embedded `0x00`, `0x80`, and `0xff`, are supported;
- the function returns ranges into the caller's string view and does not retain the input.

No normalization, Unicode collation, minimal-rotation API, suffix-index API, or mutable-string claim is implied.

## Algorithm and proof obligation

For the current un-emitted suffix beginning at `begin`, Duval maintains a scan position `scan` and a comparison position `compare` inside the candidate block.

- if `text[compare] < text[scan]`, the comparison restarts at `begin`;
- if the bytes are equal, `compare` advances;
- if `text[compare] > text[scan]`, the inner scan stops;
- when it stops, `scan - compare` is the next Lyndon period length and every copy whose start is at most `compare` is emitted.

The outer invariant is that `[0, begin)` is already the canonical Chen-Fox-Lyndon factorization. The inner scan establishes the lexicographically least primitive block for the next portion, and repeated copies of that block are emitted before the outer scan continues. Correctness and uniqueness rely on the classical Chen-Fox-Lyndon factorization theorem and Duval scan argument; tests are implementation evidence rather than substitutes for those results.

The direct implementation runs in `O(n)` byte comparisons / index work and uses `O(1)` auxiliary algorithm state plus `O(f)` returned ranges for `f` factors. It does not claim Unicode-aware ordering or a smaller-than-output space bound.

## Independent verification

The focused exact candidate passed before upload under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with halting on sanitizer failures: 4/4.

Deterministic coverage includes empty input, `banana`, repeated equal bytes, increasing and decreasing strings, embedded NUL, and high-bit byte ordering.

The primary small-instance oracle does not implement Duval. For every string over the three-byte alphabet `{0x00, 0x80, 0xff}` of length at most seven (3,280 strings total), tests enumerate every possible partition of the string. Each candidate factor is declared Lyndon only by the independent definition that it is strictly lexicographically smaller than every non-trivial cyclic rotation. The oracle accepts only partitions whose factors are all Lyndon and non-increasing; the Chen-Fox-Lyndon factorization must be the unique accepted partition, and production must match it range-for-range.

A separate 700-case fixed-seed corpus covers arbitrary bytes at lengths up to 128 and replays contiguity, Lyndon-by-rotation, and factor-order properties. This randomized corpus is supplemental evidence, not the source of the uniqueness or linear-time theorem.

## Scope

Exactly four paths differ from the recovered main checkpoint:

- `include/algorithms/strings/lyndon_factorization.hpp`;
- `tests/test_lyndon_factorization_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof/coverage document.

No CMake, README, historical ROADMAP, recovery authority, workflow, benchmark, frozen compiler/backend, minimum-cycle-basis, or temporary-file surface is modified. The connector creates file-granular commits on the feature branch; merge with squash so `main` receives one coherent recovery checkpoint.
