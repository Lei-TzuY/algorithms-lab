# Scope recovery: replayable skip-list ordered set

## Coverage decision

A fresh live-state audit at `main@bfa3c29d6a4af3c550927601082739822270ca06`
after the merged scapegoat-tree checkpoint found no skip-list implementation in
default-branch code, pull-request title history, or branch names. The exact merged-
main CI run `34768475968` completed successfully on GCC release, Clang release,
and GCC ASan+UBSan before this slice was promoted.

This work follows `docs/scope_recovery_after_phase69.md`. The Phase 45–69
compiler/backend history remains buildable but frozen; the historical ROADMAP and
recovery authority are intentionally untouched.

The nearby scapegoat-tree set restores deterministic height through occasional
whole-subtree rebuilding. A skip list adds a different proof model: ordered-set
search paths arise from independently promoted towers, and logarithmic behavior
is expected rather than a per-operation deterministic height guarantee. This is
therefore not another balancing-policy variant of the preceding recovery slice.

## Production contract

`algorithms::data_structures::SkipListSet` is a first-principles unique signed
`int64_t` ordered set.

- `insert`, `erase`, and `contains` implement set semantics;
- duplicate insertion and absent erasure return `false`;
- `values_in_order()` exposes the exact sorted level-zero witness;
- `tower_heights_in_order()` exposes the complete tower-shape witness;
- `active_level_count()` and `promotion_draw_count()` expose replay diagnostics;
- `valid_structure()` verifies strict level-zero ordering, exact node count,
  active-level bookkeeping, and that every upper level is exactly the subsequence
  of level-zero nodes whose towers reach that level;
- the implementation is noncopyable and nonmovable so ownership and pointer
  topology are unambiguous.

The implementation caps tower height at `kMaxLevel = 64`. That is an explicit
engineering bound, not a claim that an unbounded mathematical skip list can never
produce a taller tower.

## Promotion / replay invariant

Each successful insertion starts with height one. While the height is below the
implementation cap, one raw `std::mt19937_64` output is consumed: an odd low bit
promotes the tower by one level and an even low bit stops promotion.

Using the engine output directly is deliberate. No `std::bernoulli_distribution`
or other library-defined bounded-distribution mapping participates in the public
replay contract. Therefore the same seed and same sequence of successful insertions
produce the same tower heights under conforming `mt19937_64` implementations.
Duplicate insertion is detected before promotion and consumes no random draw.
Erasure is deterministic and also consumes no promotion state.

For every level, forward pointers contain exactly the sorted keys whose tower
height reaches that level. Search descends from the highest active level and moves
right while the next key is smaller than the target. Insertion splices one new
node through all levels of its sampled tower; erasure removes that node from every
level it occupies. These are the structural invariants replayed by
`valid_structure()`.

## Complexity / probabilistic boundary

Under independent probability-1/2 promotion, the classical skip-list analysis
gives expected `O(log n)` search, insertion, and erasure and expected `O(n)` total
forward-pointer storage. The explicit 64-level cap bounds implementation height.
An individual operation still has `O(n)` worst-case time on an unfavorable tower
realization; no deterministic logarithmic per-operation claim is made.

`values_in_order()`, `tower_heights_in_order()`, and `valid_structure()` are
linear diagnostic traversals. Finite randomized tests are implementation evidence
only; they do not prove the expectation theorem or a numeric high-probability
height bound.

## Independent verification

Focused repository-style verification passed before upload:

- GCC C++20 strict warnings-as-errors: 5/5;
- Clang C++20 strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with leak detection and fail-fast options: 5/5.

Deterministic evidence covers empty/basic semantics, duplicate insert, absent
erasure, `INT64_MIN` / `INT64_MAX`, 4,096 sorted insertions followed by deletion
of every even key, and exact same-seed tower replay. A dedicated regression proves
that failed duplicate insertion does not consume promotion RNG: two same-seed
lists remain byte-for-byte equivalent at the public tower-witness level even when
one list receives extra duplicate-insert attempts.

The primary randomized oracle is an independent `std::set<int64_t>`: one
fixed-seed 30,000-operation trace mixes insertion, erasure, and membership over a
bounded key domain. Every step checks structural validity and exact size; return
values are compared to the oracle, and periodic/final checks compare the complete
sorted key sequence.

## Scope / non-claims

Exactly four paths change:

- `include/algorithms/data_structures/skip_list_set.hpp`;
- `tests/test_skip_list_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this recovery proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or unrelated recovery surface is modified. This slice
adds no map/multiset API, order statistics, lock-free/concurrent behavior,
persistence, cache-performance claim, configurable promotion probability, or
cryptographic-randomness claim.
