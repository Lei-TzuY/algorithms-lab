# Phase 24 sealing audit

## Integrated checkpoint

Phase 24 is sealed only after query-local memoized run-sampled locating reached `main@f0c40c1ec242c6eba86e4acfce8535cb4ec8233b` and merged-main CI run `34179454882` completed successfully under GCC release, Clang release, and GCC ASan+UBSan.

The implementation PR used an exact six-file scope and was squash-merged only after its own GCC/Clang/sanitizer matrix passed. The merged-main run repeated the full repository suite rather than treating PR green as integration green.

## Correctness and integration audit

`BwtByteIndex::locate_run_sampled_memoized(pattern)` preserves the sealed exact backward-search interval and changes only how conceptual rows inside that interval obtain suffix positions.

The audit confirmed that:

- the query cache is seeded only from the conceptual sentinel (`SA=0`) plus sealed Phase-22 run-boundary suffix-position samples;
- Phase-20 periodic sampled-row membership and periodic sample positions are not consulted by the memoized method;
- every LF transition originates from a row that is unknown in the query cache at that moment;
- a traversed row is filled exactly once when its path reaches an already known row;
- reverse path filling uses the exact `SA(source) = SA(LF(source)) + 1 (mod n+1)` recurrence;
- the implementation fail-closes unless `lf_steps == memoized_row_count - seeded_row_count` and the total newly traversed rows do not exceed the number of initially unknown conceptual rows;
- the empty-pattern interval exercises all conceptual rows and therefore provides an executable equality case for the global LF-step bound;
- complete sorted occurrence output remains equal to independent direct scan and selected sealed Phase-23 run-sampled locate results;
- dense and deliberately sparse Phase-20 periodic sampling configurations return identical Phase-24 positions and identical memoization diagnostics, making accidental periodic-sample dependence observable.

No arbitrary-byte, conceptual-sentinel, cache-accounting, exactness, integration, sanitizer, or representability blocker was found.

## Complexity and claim audit

The sealed implementation claims the conservative query bound

`O(m log(R + 1) + n log(R + 1) + occ log occ)`

with `O(n + occ)` query workspace and unchanged `O(R)` resident run-boundary suffix-position sampling state.

This is intentionally not an r-index bound. The implementation does not claim compressed construction, universally short LF distances, persistent cross-query cache state, or a resident `O(n)` suffix-position table.

The diagnostics measure structural query work. They are not wall-clock benchmark measurements and are not used to make a performance-throughput claim.

## Why Phase 24 stops here

Several superficially adjacent changes would not introduce a new algorithmic model:

- replacing the query cache vector with a hash table;
- persisting the same memoized row positions across queries;
- adding more LF/cache counters;
- changing matched-row processing order without a new correctness/query abstraction;
- splitting the same method into another locate wrapper.

Those would primarily farm variants around the same Phase-24 memoization argument.

## Promotion decision

The next substantial frontier is **Phase 25 — bidirectional BWT exact interval extension**.

Unlike another locate optimization, a bidirectional index changes the query algebra: it maintains coordinated suffix-array intervals for a pattern and its reversal and supports extending the represented pattern by an arbitrary byte on either side. A bounded-alphabet baseline may spend explicit alphabet work per extension, but it must:

- maintain exact paired-interval cardinality invariants;
- support both left and right extension from a common empty state;
- preserve arbitrary-byte and conceptual-sentinel semantics;
- verify every extension sequence and final count against an independent direct-scan oracle;
- reuse the sealed BWT occurrence machinery rather than introducing a second unrelated suffix-index implementation;
- make no r-index, approximate-matching, compressed-construction, or asymptotically optimal bidirectional-index claim unless separately implemented and evidenced.

That is a coherent new query capability and therefore qualifies for promotion under Permanent Conquest Mode.
