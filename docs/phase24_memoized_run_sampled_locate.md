# Phase 24 — query-local memoized run-sampled BWT locating

## Goal

Phase 24 preserves the sealed Phase-22 resident run-boundary suffix-position samples and the sealed Phase-23 exact backward-search semantics, but changes the per-query time/query-memory point. Instead of resolving every matched conceptual BWT row through an independent LF walk, `BwtByteIndex::locate_run_sampled_memoized()` allocates an `O(n)` query-local row-position cache and shares resolved LF paths within that one query.

This phase does **not** add resident suffix-position samples, does not consult the Phase-20 periodic locate samples, and does not claim r-index locate bounds or compressed construction.

## Public result and diagnostics

`MemoizedRunSampledLocateResult` returns:

- `positions`: the complete sorted exact occurrence set;
- `seeded_row_count`: conceptual rows known before any query LF traversal;
- `lf_steps`: LF transitions actually executed by this query;
- `memoized_row_count`: conceptual rows with a known suffix position when the query ends;
- `matched_row_cache_hits`: matched interval rows that were already memoized before their resolution began.

The diagnostics are correctness/accounting evidence, not wall-clock benchmark data.

## Seed invariant

Let `N = n + 1` be the number of conceptual BWT rows.

The query cache starts unknown everywhere except:

1. the conceptual sentinel row, whose suffix position is exactly `0`; and
2. every sealed Phase-22 full-conceptual-BWT run-boundary sample, with its stored suffix position.

Therefore

`seeded_row_count = 1 + run_toehold_sample_count()`.

The Phase-20 `sampled_rows_` and `sampled_positions_` structures are never read by this method. Dense and sparse periodic-locate configurations over the same text must therefore produce identical positions **and identical memoization diagnostics**.

## LF path-compression invariant

For each row in the exact backward-search interval:

1. if the row is already memoized, no LF step is required for that row;
2. otherwise, follow LF only while the current conceptual row is unknown, recording each unknown source row exactly once in a query-local path;
3. stop as soon as an already memoized row is reached;
4. if the known row has suffix position `p`, fill the recorded path backwards with the inverse LF/SA recurrence

   `previous_position = (p + 1) mod N`;

5. every row filled during that unwind becomes available to all later matched rows in the same query.

Because an LF transition is executed only from an unknown row and that source row is memoized before the walk finishes, a conceptual row can be the source of a newly traversed LF step at most once per query.

The implementation fail-closes unless

`lf_steps = memoized_row_count - seeded_row_count`

and

`lf_steps <= N - seeded_row_count`.

For the empty pattern, the exact interval contains every conceptual row, so the finished cache must cover all `N` rows and the bound becomes an equality:

`lf_steps = N - seeded_row_count`.

## Exactness

Backward search is unchanged from the sealed BWT index. The new method changes only how rows in the resulting interval obtain suffix positions.

The seed positions are exact by the Phase-22 construction. LF maps suffix-array position `p` to `p - 1 (mod N)`, so reversing one recorded LF edge by adding one modulo `N` preserves the exact suffix position. Induction along each reversed recorded path therefore gives the exact suffix position for every newly memoized row. Sorting those positions produces the same complete exact occurrence set as direct substring scan and the sealed Phase-23 `locate_run_sampled()` path.

## Complexity and storage boundary

Let:

- `n` be text length;
- `N = n + 1` conceptual BWT rows;
- `R` be the number of resident full-BWT runs / run-boundary-sampling scale;
- `m` be pattern length;
- `occ` be the number of matches.

The sealed run-length occurrence representation gives logarithmic run-index operations. Backward search remains conservatively `O(m log(R + 1))`.

Within one memoized locate query, each previously unknown conceptual row can trigger at most one LF transition, so traversal is conservatively `O(n log(R + 1))`. Sorting the returned positions is `O(occ log occ)`. The phase therefore claims the conservative query bound

`O(m log(R + 1) + n log(R + 1) + occ log occ)`.

The query allocates `O(n)` row-position memoization plus path workspace and `O(occ)` output. Resident run-boundary suffix-position sampling remains `O(R)` and construction is unchanged from the sealed index.

No claim is made that every individual occurrence has a short LF distance, that resident state is an r-index, or that the construction itself is compressed-space.

## Verification

Deterministic tests cover:

- empty text and empty/absent patterns;
- `banana` exact matches;
- highly repetitive input;
- a conceptual-sentinel run-split adversary;
- arbitrary bytes including `0x00`, `0x80`, and `0xff`;
- exact equality with Phase-23 complete run-sampled locating;
- full-cache accounting for the empty pattern.

A fixed-seed randomized differential suite uses 350 texts of length `0..80`, two deliberately different Phase-20 periodic sample rates, and 60 patterns per text. Every result is compared with an independent direct substring scan. The dense and sparse indexes must also have identical Phase-24 diagnostics, making accidental dependence on the periodic samples executable-visible.

Randomized equality is implementation evidence. The exactness and traversal bounds come from the seed/LF/cache invariants above.

## Phase status

Phase 24 is **SEALED** after the implementation reached `main@f0c40c1ec242c6eba86e4acfce8535cb4ec8233b` and merged-main CI run `34179454882` passed GCC release, Clang release, and GCC ASan+UBSan. The final architecture audit is recorded in `docs/phase24_sealing_audit.md`.

The next frontier is Phase 25 bidirectional BWT interval search: maintain paired exact intervals for a pattern and its reversal so the represented pattern can be extended on either the left or the right. That is a new query model rather than another memoization variant; it must preserve arbitrary-byte semantics, expose exact interval cardinality, and be verified against independent direct scan without claiming compressed-optimal bidirectional-index bounds.
