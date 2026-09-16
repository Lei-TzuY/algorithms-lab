# Scope recovery: bounded exact consecutive-ones ordering

## Coverage decision

A fresh post-Phase-69 coverage audit after the Queyranne symmetric-submodular
checkpoint found no consecutive-ones-property recognizer, PQ-tree / PC-tree
implementation, or occupied consecutive-ones recovery branch. This slice adds a
matrix-ordering / interval-hypergraph proof model rather than another matroid,
submodular, graph-cut, predecessor, or dynamic-tree variant.

## Production contract

`consecutive_ones_ordering(column_count, one_columns_by_row)` accepts a binary
row/column incidence matrix encoded as the column ids containing a one in each
row.

- at most 20 columns in this deliberately bounded exact baseline;
- empty matrices, empty rows, singleton rows, and repeated identical rows are
  valid;
- a duplicate column id inside one row is malformed and rejects;
- out-of-range column ids reject;
- success returns the lexicographically smallest column permutation in which
  every row's ones occupy one contiguous interval;
- failure returns `nullopt`;
- diagnostics expose memoized states explored and candidate-column transitions
  checked.

The implementation does not mutate caller input and does not call a library
consecutive-ones / interval-hypergraph recognizer.

## State invariant and exact recurrence

For a valid partial permutation with used-column set `S`, each row `R` is in one
of three states determined by `S` alone:

1. untouched: `S intersect R` is empty;
2. open: `S intersect R` is non-empty and is not all of `R`;
3. complete: `R` is a subset of `S`.

A row in the open state has started its unique consecutive block but has not
finished it. Therefore the next column must belong to every open row. Conversely,
if a candidate column belongs to every currently open row, appending it preserves
partial validity: untouched rows may stay untouched or start, open rows stay open
or complete, and complete rows impose no future restriction.

This gives an exact recurrence whose memoization key is only the used-column bit
mask. Trying candidate columns in ascending order and retaining the first
successful continuation yields the lexicographically smallest valid witness.
The proof obligation is this prefix invariant and the necessity/sufficiency of
the transition rule; finite tests are implementation evidence rather than a
replacement for the argument.

## Complexity and non-claims

For `n <= 20` columns and `r` distinct row masks, there are at most `2^n`
memo states. Each state tries at most `n` next columns and scans the row masks, so
the direct bound is `O(2^n * n * r)` time and `O(2^n + r + n)` resident/result
space, plus input normalization.

This is intentionally **not** a Booth-Lueker PQ-tree, PC-tree, linear-time
consecutive-ones recognizer, circular-ones recognizer, Tucker-obstruction
extractor, or canonical PQ-tree representation. The column bound and exponential
cost are part of the public educational contract, not hidden implementation
details.

## Independent verification

The primary bounded oracle enumerates column permutations in lexicographic order
and directly checks whether each row's one positions form a contiguous interval.
It shares neither the production used-mask recurrence nor the open-row
transition rule.

Committed evidence covers malformed rows, empty/trivial constraints, the public
20-column boundary, incompatible pair constraints, deterministic lexicographic
witness semantics, and 320 fixed-seed random matrices with 0..8 columns and
0..12 rows. Production feasibility must equal exhaustive permutation feasibility;
on success the complete witness must also equal the oracle's first valid
permutation.

Focused exact candidate bytes pass repository-equivalent GCC and Clang C++20
strict warnings-as-errors plus actual GCC ASan+UBSan execution.
