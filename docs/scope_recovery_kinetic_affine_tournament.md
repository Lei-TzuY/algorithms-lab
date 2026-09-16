# Scope recovery: discrete kinetic affine tournament

## Coverage decision

A fresh live audit at `main@46868a3104ad4f388b179e83f4afd4d957b01c51`
found no kinetic-data-structure, moving-trajectory, kinetic-tournament, or
certificate-failure surface in default-branch code, PR history, or branch names.
The immediately preceding recovery checkpoint is a partially retroactive FIFO
queue; this slice deliberately changes proof model rather than extending that
same temporal-edit surface. Prospective governance remains
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical ROADMAP are untouched.

## Production contract

`DiscreteKineticAffineTournament` owns a fixed collection of affine trajectories

`f_i(t) = slope_i * t + intercept_i`

and maintains their minimum while integer time moves monotonically forward.

- slope, intercept, and public time are restricted to `[-1e9, 1e9]`;
- ties use the smallest trajectory id;
- `minimum()` returns exact `(trajectory,value)` at the current time;
- `advance_to(t)` accepts only nondecreasing in-domain integer time;
- `next_certificate_failure_time()` exposes the next currently scheduled local
  certificate failure as an `O(n)` diagnostic;
- `processed_certificate_failures()` makes the event-driven execution replayable;
- `valid_structure()` independently replays leaves, every tournament winner,
  every local certificate time, the event-heap invariant, and the root against a
  direct full scan.

This is intentionally a **discrete integer-time** kinetic data structure. It does
not claim continuous real/rational event semantics between integer timestamps.
Trajectories are immutable after construction.

## Certificate invariant

Each internal tournament node stores the preferred winner of its two child
winners under lexicographic `(value,id)` order at the current time. The losing
child winner has one affine difference from the winner. If its relative slope is
nonnegative, it can never take over in the future. Otherwise production computes
exactly the first future integer time when either

- the loser has strictly smaller value, or
- values tie and the loser has smaller id.

Because an affine difference is monotone, the certificate is valid at every
integer time before that failure. The root is therefore the global minimum while
all internal certificates are valid. Processing the earliest valid failure and
recomputing only its ancestor path restores the invariant. Simultaneous events
prioritize descendants before ancestors; generation numbers invalidate stale
ancestor events after a descendant changes.

The event queue reuses the repository's first-principles Phase-1 `BinaryHeap`.
Stale events are periodically compacted from the authoritative per-node schedule
so resident queue state remains linear-scale rather than growing with the whole
history.

## Exact arithmetic boundary

At the public bounds,

`|slope * time + intercept| <= 10^18 + 10^9`,

and pairwise slope/intercept differences are at most `2e9` in magnitude. The
integer crossing calculation therefore remains inside signed 64 bit without
floating point or non-standard wider integers. Out-of-domain coefficients/times
fail closed.

## Complexity / non-claims

Let `n` be the trajectory count and `F` the number of valid certificate failures
processed over an execution prefix.

- current minimum: `O(1)`;
- construction: `O(n log n)` direct heap work in this baseline;
- one valid certificate failure repairs `O(log n)` tournament nodes, each with a
  heap reschedule, giving `O(log^2 n)` direct heap work before occasional linear
  event-queue compaction;
- over a sequence, stale-event creation/popping and periodic rebuilding are
  charged to those reschedules; storage is `O(n)` after compaction;
- `valid_structure()` and `next_certificate_failure_time()` are intentionally
  diagnostic and may scan `O(n)` state.

No continuous-time KDS, trajectory insertion/deletion, arbitrary polynomial
motion, event-count optimality, high-probability bound, wall-clock benchmark, or
universal speedup over direct scanning is claimed.

## Verification

Focused final bytes passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 6/6;
- Clang C++20 repository strict warnings-as-errors: 6/6;
- actual GCC ASan+UBSan with fail-fast/leak detection: 6/6.

Deterministic evidence covers empty/domain validation, exact public arithmetic
bounds, crossings on and between integer timestamps, id tie-breaking,
simultaneous certificate failures, monotone-time rejection, replay determinism,
and certificate diagnostics.

Primary randomized evidence uses 500 fixed-seed trajectory sets with 0..32 lines
and 180 monotone time advances each. At every sampled time an independent direct
full scan computes the exact `(value,id)` minimum, while production structure and
certificate invariants are replayed. The oracle does not use tournament state,
certificate scheduling, or the event heap.

During focused development one test incorrectly expected a failure at a value tie
where the existing smaller-id winner should remain preferred. The oracle
expectation was corrected; production semantics were unchanged. The regression
now explicitly distinguishes equality from an actual certificate failure.
