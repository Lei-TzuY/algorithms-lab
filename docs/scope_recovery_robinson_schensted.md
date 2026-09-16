# Scope recovery — Robinson-Schensted permutation correspondence

## Coverage decision

A fresh live audit after the Garsia-Wachs alphabetic-coding checkpoint found no
Robinson-Schensted / RSK / Young-tableau production surface in default-branch
code, pull-request history, or branch names. The occupied BFPRT-selection and
Count-Sketch branches are deliberately untouched. Prospective authority remains
`docs/scope_recovery_after_phase69.md`; the stale historical compiler/backend
ROADMAP is not resumed.

This slice changes proof model again: from optimal ordered coding to a bijective
combinatorial correspondence whose shape simultaneously encodes subsequence
extrema.

## Production contract

`robinson_schensted_permutation(permutation)` accepts exactly a permutation of
`[0,n)` and returns a pair `(P,Q)` of equal-shape standard Young tableaux:

- `P` is the row-insertion tableau containing the permutation values;
- `Q` records the insertion time at the unique box created by each insertion;
- rows and columns are strictly increasing;
- row lengths form a partition;
- empty input maps to the empty tableau pair.

Malformed inputs (duplicate or out-of-range values) reject with
`std::invalid_argument`.

`inverse_robinson_schensted_permutation(result)` validates the complete standard
pair and performs reverse row insertion from recording labels `n-1..0`, returning
the unique original permutation. `valid_robinson_schensted_result` exposes the
shape/standardness invariant as an executable diagnostic.

## Correctness / theorem boundary

Forward insertion places a carried value into the first row position whose value
is larger, bumping the displaced value to the next row until one new outer-corner
box is created. Recording labels are attached only to those newly created boxes.
Reverse insertion removes recording labels in descending order; each removed
outer corner carries a value upward by replacing the rightmost smaller value in
the preceding row. These operations are inverse to each other.

The classical Robinson-Schensted theorem gives a bijection between permutations
and equal-shape pairs of standard Young tableaux. Schensted's subsequence theorem
further states that the first row length equals the longest increasing
subsequence length and the first-column length (the number of rows) equals the
longest decreasing subsequence length.

Those mathematical results are proof obligations. The finite verification below
is implementation evidence, not a proof of the bijection or subsequence theorem.

## Verification

Focused exact candidate bytes pass:

- GCC C++20 repository-equivalent strict warnings-as-errors: 3/3;
- Clang C++20 repository-equivalent strict warnings-as-errors: 3/3;
- actual GCC ASan+UBSan with fail-fast/leak detection: 3/3.

Committed evidence covers empty input, increasing/decreasing permutations, a
hand-replayable mixed permutation, malformed permutation rejection, malformed
pair rejection, deterministic replay, and exact forward/inverse reconstruction.

The primary finite oracle has two independent layers:

1. every permutation for `n=0..7` is enumerated; returned tableaux are checked by
   an independent standard-tableau validator and inverse replay must recover the
   exact permutation;
2. independent quadratic dynamic programs compute strict LIS and LDS lengths and
   must equal the returned shape's first-row length and row count.

A separate 1,200-case fixed-seed corpus shuffles permutations of sizes `0..64`
and repeats the same structural, inverse, and subsequence checks. Neither oracle
uses row insertion or reverse bumping.

A supplementary local ASan+UBSan stress corpus replayed 20,000 additional
permutations of sizes `0..128`; this is robustness evidence only and is not added
to the already-large full suite.

## Complexity / non-claims

This direct educational baseline linearly scans tableau rows during insertion and
reverse insertion, so both directions use `O(n^2)` worst-case time and `O(n)`
result/working storage. Validation is linear in the number of stored boxes apart
from allocation bookkeeping.

No semistandard-word RSK, Knuth equivalence, Greene's theorem, jeu de taquin,
representation-theoretic counting, hook-length implementation, asymptotically
faster insertion, or benchmark-speedup claim is made.

## Scope

The intended recovery diff is exactly four paths:

- `include/algorithms/combinatorial/robinson_schensted.hpp`;
- `tests/test_robinson_schensted_cases.hpp`;
- one registry include in `tests/test_main.cpp`;
- `docs/scope_recovery_robinson_schensted.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, BFPRT, Count-Sketch, or temporary-file churn is needed.
