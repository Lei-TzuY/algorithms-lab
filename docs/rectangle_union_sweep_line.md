# Exact axis-aligned rectangle union area

## Contract

`rectangle_union_area` accepts axis-aligned rectangles with coordinates in the
repository's bounded exact domain `[-1'000'000'000, 1'000'000'000]`. Each
rectangle uses half-open geometry `[min_x,max_x) x [min_y,max_y)` for accounting;
this convention changes no Euclidean area but makes touching boundaries
unambiguous. Reversed bounds are rejected. Zero-width or zero-height rectangles
are valid and contribute zero area.

The result is the exact signed `int64_t` union area. The accepted coordinate
box has width and height at most `2e9`, so every possible union is bounded by
`4e18 < INT64_MAX`. No floating point, epsilon, non-standard wide integer, or
modular arithmetic is used.

## Sweep invariant

Create an entering event at `min_x` and a leaving event at `max_x` for every
non-degenerate rectangle. Events sharing one x coordinate are processed as a
batch. Immediately before processing the batch at `x`, the covered-length tree
represents exactly the y-union of rectangles active on the preceding open
x-slab. Therefore

`(x - previous_x) * covered_y_length`

is exactly the newly swept area. Only after that slab is charged are all events
at `x` applied. Event tie order inside the zero-width batch cannot alter area.

## Covered-length tree invariant

All rectangle y endpoints are coordinate-compressed. Leaves represent elementary
intervals `[y_i,y_{i+1})`, so no rectangle boundary lies inside a leaf.

Each segment-tree node stores:

- a cover multiplicity for updates that fully cover the node interval;
- the exact geometric length covered at least once in that interval.

If the node cover count is non-zero, its entire coordinate span is covered. If
the count is zero, a leaf contributes zero and an internal node equals the sum
of its children. Parallel/duplicate rectangles are therefore handled by cover
multiplicity rather than by double-counting area.

The compressed y span is at most `2e9`, so every stored covered length is exactly
representable in `int64_t`.

## Complexity

For `n` rectangles, there are at most `2n` x-events and `2n` y coordinates.
Sorting and coordinate compression cost `O(n log n)`. Every non-degenerate
rectangle contributes two `O(log n)` covered-length updates. Total time is
`O(n log n)` and auxiliary storage is `O(n)`.

This is a first-principles sweep/covered-length implementation. Standard sorting
is used only to order events and compression coordinates; sorting itself is not
the algorithm under study.

## Verification

Deterministic regressions cover empty input, zero-area rectangles, overlap,
containment, duplicate rectangles, touching edges, same-x event batches, input
order reversal, full-domain area `4e18`, reversed bounds, and out-of-domain
coordinates.

An exhaustive small catalog compares every pair drawn from all positive-area
rectangles on a `2 x 2` integer grid with an independent unit-cell oracle. A
fixed-seed randomized corpus adds 2,000 multisets of up to 12 rectangles with
integer coordinates in `[-5,5]`; every result is compared with independent
unit-cell enumeration and then replayed after input shuffling.

Focused pre-upload builds pass repository-equivalent GCC strict warnings, Clang
strict warnings, and actual GCC ASan+UBSan. Exact PR-head full-repository CI and
merged-main CI remain the integration gates.
