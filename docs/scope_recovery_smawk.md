# Scope recovery: SMAWK totally-monotone row minima

## Coverage decision

A fresh live-state audit after the exact prime-field discrete-logarithm recovery
found no SMAWK implementation, no totally-monotone row-minimum API, no matching
branch, and no historical pull request in `algorithms-lab`. Several adjacent
classical surfaces are already occupied by other recovery branches, so this
slice deliberately changes proof model instead of competing with graph,
number-theory, or data-structure work already in flight.

## Contract

`algorithms::optimization::smawk_row_minima` accepts a dense rectangular matrix
of signed 64-bit values and returns the **leftmost** minimum column index for each
row.

- an empty matrix returns an empty result;
- a non-empty matrix must have at least one column;
- ragged matrices are rejected;
- total monotonicity is a semantic precondition.

The implementation intentionally does not run a general total-monotonicity
validator. Such validation would dominate the linear matrix-access complexity
that SMAWK is meant to study. Inputs that violate the precondition therefore
have no promised row-minimum result; some ordering inconsistencies are rejected
when encountered, but this is not a validator claim.

## First-principles invariant

For an active row list and ordered candidate-column list:

1. **Column reduction.** When the reduced stack currently contains `k` columns,
   a new column is compared at active row `rows[k-1]`. If the new value is
   strictly smaller, the old top can never be the leftmost row minimum needed by
   the recursive problem and is removed. Equality keeps the earlier column, so
   tie semantics remain leftmost. The reduced list contains at most as many
   columns as active rows.
2. **Recursive odd rows.** SMAWK recursively solves rows 1, 3, 5, ... over the
   reduced columns. Total monotonicity implies these solved minimum-column
   positions are nondecreasing.
3. **Even-row interpolation.** The minimum for an even row lies between the
   solved minima of its neighboring odd rows (or a matrix boundary). A monotone
   forward scan over the reduced columns therefore fills all remaining rows
   without restarting searches from column zero.

This is the classical SMAWK reduction/recursion/interpolation structure; no
library row-minimum routine or hidden full scan is used by production.

## Complexity

For `R` rows and `C` columns with O(1) dense-matrix access, the implementation
uses `O(R + C)` matrix comparisons/accesses and `O(R + C)` auxiliary storage
under the total-monotonicity precondition. Shape validation itself is `O(R)`.

The interpolation code uses monotone pointers, not binary search or hashing, so
it does not silently turn the intended linear bound into `O(R log C)`.

## Verification

Focused repository-style verification passed:

- GCC C++20 with the repository strict warning set;
- Clang C++20 with the same warnings-as-errors;
- GCC ASan+UBSan.

Deterministic regressions cover empty input, zero-column/ragged rejection,
single-row and single-column matrices, and leftmost tied minima.

The primary differential corpus contains 1,200 fixed-seed rectangular Monge
matrices with independently chosen row counts and column counts in `[1, 90]`.
Their nondecreasing quadratic centers produce both strict and tied row minima.
Every SMAWK result is compared against an independent naïve full-row scan, and
the returned minimum indices are additionally checked to be nondecreasing.
Monge matrices are a strict well-structured subclass used as executable evidence;
the broader totally-monotone contract is justified by the SMAWK invariant above,
not by claiming this finite corpus exhausts that class.

## Non-claims

This slice does not provide a total-monotonicity recognizer, a generic callback
matrix abstraction, Monge optimization beyond row minima, benchmark-backed speed
claims, or a claim that arbitrary matrices receive correct minima in linear
work. It is an exact first-principles SMAWK row-minimum implementation under its
stated semantic precondition.
