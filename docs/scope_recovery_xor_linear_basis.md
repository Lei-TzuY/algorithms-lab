# Scope recovery: 64-bit XOR linear basis

## Coverage decision

A fresh live audit at `main@4c129d3ab9a67abc2f940bcad8530bcf5bbb5f23`
found no XOR / GF(2) linear-basis implementation in default-branch code, pull-
request history, or branch names. The currently live HyperLogLog and persistent-
DSU recovery branches are left untouched, and full SPQR remains too broad for one
independently verifiable recovery slice.

This checkpoint deliberately changes proof model after the recent soft-heap,
quotient-filter, and disjoint-sparse-table recoveries: finite-dimensional vector
spaces over GF(2), rather than another heap, approximate-membership structure, or
range index. Prospective authority remains `docs/scope_recovery_after_phase69.md`;
the frozen compiler/backend history and stale historical ROADMAP are untouched.

## Production contract

`algorithms::data_structures::XorLinearBasis64` is a first-principles basis for
64-bit vectors under XOR.

- `insert(x)` adds `x` iff it increases span rank;
- `rank()` is the dimension of the represented span;
- `contains(x)` answers exact span membership;
- `canonical_basis()` returns the reduced pivot rows in descending pivot order;
- `maximize_xor(seed)` returns the maximum unsigned value `seed XOR x` over all
  represented span elements `x`;
- zero is always representable and never increases rank;
- the complete resident representation is exactly 64 `uint64_t` pivot slots plus
  rank metadata; no hidden copy of inserted values is retained.

## Invariant and proof boundary

Every nonzero resident row has a distinct highest set bit (its pivot). Insertion
eliminates existing higher pivots, clears all existing lower pivot columns from
the new row, installs the new pivot, then clears that pivot column from every
higher row. Consequently every pivot column contains a one in exactly one row.

XORing one row into another is an elementary row operation over GF(2), so these
reductions preserve the represented span. A candidate reduces to zero exactly
when it lies in that span. Because pivot columns are fully reduced, the stored
rows form the unique reduced-row-echelon representation for the subspace under
the fixed bit order; insertion order therefore cannot change `canonical_basis()`.

For `maximize_xor`, scanning pivots from bit 63 down to 0 and accepting an XOR
exactly when it increases the unsigned value greedily fixes the highest not-yet-
fixed bit. Lower pivots cannot change an already decided higher bit, yielding the
global maximum over the represented affine coset `seed XOR span`.

These finite-field / greedy arguments are the proof obligations; randomized tests
are implementation evidence rather than theorem substitutes.

## Verification

Focused final candidate bytes passed before upload:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast leak/UB checks: 4/4.

Deterministic evidence covers empty/zero semantics, dependence rejection, bit 63,
seeded maximization, structural diagnostics, and insertion-order-independent
canonicalization.

Primary randomized verification uses 1,200 fixed-seed input multisets of at most
10 sixteen-bit vectors. The independent oracle explicitly enumerates every subset
XOR into a `std::set`; production rank, 256 membership probes, and seeded maximum
must match the exact enumerated span. A separate 500-trial full-width corpus
shuffles up to 63 random `uint64_t` values and requires identical canonical
reduced bases before and after reordering.

## Complexity and non-claims

The domain width is fixed at 64. Insertion performs at most two 64-pivot scans;
membership and maximization perform one. Thus public operations are constant
bounded work for this API (equivalently `O(w^2)` insertion and `O(w)` query for
word width `w=64`), and resident storage is `O(w)`.

No arbitrary-width bit-vector basis, deletion, persistence, weighted matroid
optimization, linear-system solver, enumeration of all represented values,
cryptographic property, benchmark speedup, or general finite-field claim is
implied.

## Scope

The recovery slice changes exactly four paths: one header-only production type,
one repo-native test header, one include line in `tests/test_main.cpp`, and this
proof/coverage document. It does not touch CMake, README, historical ROADMAP,
recovery authority, workflow, benchmark, frozen compiler/backend code, or the
occupied HyperLogLog/persistent-DSU branches.
