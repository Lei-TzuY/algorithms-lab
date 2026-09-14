# Scope recovery: exact orthogonal segment-intersection sweep

## Coverage decision

Fresh live-state recovery at `main@82ef74df4f349a27f0c96acf860336a246461b0b` found no horizontal/vertical orthogonal-intersection reporting implementation on the default branch and no matching recovery pull request. The nearest capabilities solve different problems: Phase-7 exposes an exact predicate for one arbitrary segment pair, while the sealed rectangle-union sweep aggregates covered area rather than reporting segment-pair witnesses. The immediately preceding recovery checkpoint is the y-fast predecessor set, so this slice deliberately changes proof model instead of extending the predecessor family.

The prospective authority remains `docs/scope_recovery_after_phase69.md`; the historical compiler/backend ROADMAP remains frozen prospectively.

## Production contract

`orthogonal_segment_intersections(horizontals, verticals)` accepts two orientation-separated collections of non-degenerate segments over the repository's exact `Point2i` domain `[-1e9,1e9]`.

- every element of the first collection must be horizontal;
- every element of the second collection must be vertical;
- zero-length or mis-oriented segments are rejected;
- intersections are inclusive at both horizontal and vertical endpoints;
- duplicate/geometrically coincident input segments remain distinct by input index;
- output contains one witness for every intersecting horizontal/vertical input pair;
- each witness stores the exact intersection point and both original input indices;
- output is deterministically sorted by `(x, y, horizontal_index, vertical_index)`.

Same-orientation intersection/overlap is intentionally outside the API: horizontal and vertical collections are supplied separately, avoiding an ambiguous finite-point representation for collinear overlaps.

## Sweep invariant and proof obligation

Each horizontal segment `[x_left,x_right]` produces an activation and a removal event at its fixed `y`. Each vertical segment produces one range-query event at its fixed `x` and closed y interval.

Events at equal x are ordered

`horizontal_start < vertical_query < horizontal_end`.

Therefore, immediately before every vertical query at coordinate `x`, the active structure contains exactly the horizontal segments whose **closed** x interval contains `x`. This ordering is what makes both horizontal endpoints inclusive.

The active structure is an ordered map from y coordinate to the set of active horizontal input ids. A vertical query starts at `lower_bound(y_min)` and enumerates every active y through `y_max`; each enumerated id is exactly one intersecting pair and yields point `(x_vertical,y_horizontal)`. Conversely every intersecting horizontal/vertical pair is active when that vertical query executes and its y lies inside the query interval, so no pair is omitted.

Duplicate horizontal segments are retained as distinct ids in the per-y set rather than collapsed geometrically.

## Complexity

For `H` horizontal segments, `V` vertical segments, and `K` reported pair intersections:

- event construction/sort: `O((H+V) log(H+V))`;
- horizontal activation/removal: `O(log(H+1))` per event;
- each vertical query performs one ordered lower bound plus output-proportional range traversal;
- total direct bound: `O((H+V) log(H+V) + K)` time;
- resident event/active/result state: `O(H+V+K)`.

`std::sort`, `std::map`, and `std::set` provide ordering containers; they do not implement the sweep-line algorithm under study.

## Verification

Focused pre-upload candidate passed:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak checking: 4/4.

Deterministic coverage includes malformed orientation, degenerate segments, coordinate-domain rejection, empty/no-hit cases, reversed endpoint order, inclusive endpoint intersections, duplicate pair witnesses at one geometric point, repeated deterministic execution, and the exact `+1e9` coordinate boundary.

Randomized differential evidence uses 1,200 fixed-seed horizontal/vertical multisets (0..18 segments of each orientation, coordinates in `[-15,15]`). Production output is compared exactly with an independent `O(H*V)` pair-enumeration oracle that tests closed coordinate intervals directly and does not use event ordering or an active set.

## Scope / non-claims

This recovery changes exactly four paths: one header-only implementation, one repo-native test header, one include line in `tests/test_main.cpp`, and this proof document. It does not modify CMake, README, historical ROADMAP, recovery authority, workflows, benchmarks, or frozen compiler/backend code.

This slice does not claim arbitrary-orientation Bentley-Ottmann reporting, same-orientation collinear-overlap enumeration, floating-point intersection coordinates, dynamic insert/delete queries, output-sensitive bounds for unrestricted segment arrangements, or a universal speedup over direct pair enumeration on tiny inputs.
