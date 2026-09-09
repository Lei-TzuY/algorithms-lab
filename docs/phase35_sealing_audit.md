# Phase 35 sealing audit

Phase 35 is sealed only after the exact augmented link-cut implementation reached merged `main` and the post-merge CI matrix completed successfully.

## Integrated checkpoint

- implementation candidate: `dc5e4595337395c032c42c7a252c0e28cd09d52a`;
- candidate CI run `34325300351`: GCC release, Clang release, and GCC ASan+UBSan all successful;
- integrated main commit: `aa91765285541c8e209d171d9f027b8c5941c7e7`;
- merged-main CI run `34325821036`: GCC release, Clang release, and GCC ASan+UBSan all successful.

There were no open pull requests or issues at audit time, and no parallel Phase-36 implementation branch was present.

## Correctness and representability audit

The sealed extension adds signed `int64_t` node values, point assignment, and represented-path sums without changing represented-tree topology semantics.

Internal preferred-path aggregates deliberately do not use `int64_t`. A private two-limb signed-magnitude representation keeps every structurally possible auxiliary sum exact on repository-supported platforms where `size_t` has at most 64 value bits. This prevents false rejection when an intermediate preferred-path grouping is outside `int64_t` but the final requested path sum is representable, including the deterministic `INT64_MAX + 1 - 1` cancellation regression.

The public `path_sum()` boundary narrows only the final exact result and throws only when that mathematical result lies outside `int64_t`. Point assignment accepts every `int64_t` value. Rotations, access, and point assignment all call `pull`; lazy reversal swaps children but leaves the commutative aggregate unchanged.

`valid_auxiliary_invariants()` independently recomputes auxiliary subtree sizes and exact aggregates in addition to Phase-34 parent/child, acyclicity, and coverage checks.

## Verification independence

The randomized differential does not implement another link-cut tree. It maintains an adjacency-matrix forest and reconstructs unique represented paths with BFS. The 35,000-operation fixed-seed trace mixes link, cut, point assignment, connectivity, path distance, and path sum and checks link-cut auxiliary invariants after every operation.

Deterministic tests additionally cover disconnected and bounds rejection, bidirectional sums through topology changes, exact `INT64_MIN`, positive/negative overflow, and cancellation across an out-of-range intermediate aggregate.

## Complexity claim audit

Phase 35 inherits the standard link-cut amortized `O(log V)` operation bound over a sequence and `O(V)` resident storage. It explicitly retains the Phase-34 caveat that one individual operation may take `O(V)` before amortization. The exact two-limb aggregate is constant-size per node and does not alter asymptotic storage or update/query bounds.

No CI wall-clock time or uncontrolled benchmark is used as asymptotic evidence.

## Scope decision

The phase target is complete. Adding path minimum/maximum or additional scalar aggregate variants would reuse the same already-proved augmentation pattern without introducing a comparable new architectural obligation, so those variants are not farmed before sealing.

A more substantial next frontier is lazy numeric mutation across an entire represented path. That requires a new deferred-update state machine that must compose with splay access and existing reversal tags rather than merely adding another aggregate field.

## Promotion

Phase 36 promotes to **lazy augmented dynamic trees**. The first coherent slice adds uniform represented-path assignment while preserving point assignment, dynamic topology, and exact path-sum semantics. A pending assignment tag must propagate correctly through access/rotation/reversal, and the exposed path aggregate must be set exactly to `path_length * assigned_value` in the existing wide arithmetic domain. Verification must mix topology changes, point assignments, path assignments, and path sums against an independent naïve forest/BFS oracle, including overwrite order, deferred-tag exposure, cancellation, and public overflow boundaries.
