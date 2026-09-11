# Scope recovery: exact cover with Algorithm X and Dancing Links

## Capability

`solve_exact_cover(column_count, rows)` solves the classical exact-cover problem:
select a subset of input rows so every column in `[0, column_count)` is covered
exactly once. The returned witness contains the selected input-row indices.
Unsatisfiable instances return `std::nullopt`.

Rows may be empty; empty rows are irrelevant and are never selected. Repeating a
column inside one row is rejected because it would violate the set-system model,
and column indices outside the declared universe are rejected.

## Sparse reversible state

The production solver implements Knuth's Algorithm X over a first-principles
Dancing Links representation. Column headers and row entries form circular
left/right and up/down linked lists. Covering a column removes its header from the
active header ring and removes every conflicting row entry from the other column
lists. Uncovering walks the same affected rows and entries in reverse order and
restores every removed link and column cardinality.

The core proof obligation is therefore structural: after `uncover(c)` returns,
the sparse matrix state must be exactly the state that existed immediately before
`cover(c)`. A deterministic regression forces the first candidate row down a dead
end so correctness depends on a complete cover/fail/uncover/retry sequence rather
than only on successful straight-line searches.

## Search invariant and determinism

At each recursive state, the active header ring is exactly the set of uncovered
columns. A row reachable from an active column represents an input row that is
still compatible with all already-selected rows. Selecting a row covers every
column in that row, so no later selected row can overlap it.

A recursive state with no active column is therefore a complete exact cover. An
active column with cardinality zero is a proof that the current partial choice
cannot be completed.

The implementation chooses the active column with minimum cardinality. Ties are
resolved by ascending original column index because the header ring preserves
that order. Candidate rows are visited in original input order. The heuristic
changes search order only; it does not alter the exact-cover condition.

## Verification

Deterministic cases include the standard seven-column exact-cover example,
zero-column and unsatisfiable instances, ignored empty rows, duplicate/out-of-
range validation, repeated-run determinism, and the forced failed-branch
restoration regression.

A fixed-seed corpus of 2,000 random instances uses at most eight columns and
12 rows. The primary oracle enumerates every subset of rows and independently
checks whether each column is covered exactly once. Production SAT/UNSAT must
match that exhaustive oracle. Every returned production witness is also replayed
against the original row sets, and the same input must return the same witness on
repeated calls.

Focused pre-upload builds pass the repository strict-warning policy under GCC and
Clang and an actual GCC AddressSanitizer + UndefinedBehaviorSanitizer build: 5/5
focused test groups in all three configurations.

## Complexity and non-claims

Exact cover is exponential in the worst case. The minimum-column heuristic often
reduces the practical search tree but does not provide a polynomial-time bound.
Dancing Links makes each cover/uncover proportional to the sparse entries touched
by that operation and restores them without rebuilding the matrix.

The recursive depth is bounded by the number of selected rows / covered-column
steps, so extremely deep adversarial instances can still consume process stack.
This slice does not claim a worst-case improvement over exponential exact search,
parallel search, Sudoku-specific optimizations, or enumeration of every solution.
