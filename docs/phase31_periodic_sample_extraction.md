# Phase 31 — Periodic-Sample Local BWT Extraction

## Scope

Phase 31 turns the sealed Phase-20 periodic locate samples into an exact bounded text-extraction substrate. It does not replace the sealed Phase-29 full-reconstruction API: `BwtByteIndex::extract_text()` remains the explicit `O(n)` baseline. The new `BwtPeriodicSampleTextExtractor` borrows an existing `BwtByteIndex`, builds one suffix-position-to-row inverse over exactly the same Phase-20 periodic samples, and exposes exact half-open extraction without retaining a constructor-text copy.

The extractor must not outlive the `BwtByteIndex` it references.

## Inverse-sample invariant

Phase 20 already stores:

- a packed conceptual-row membership vector for sampled rows; and
- `sampled_positions_`, whose entries are suffix positions paired with those sampled rows in row order.

The sample policy is unchanged: suffix position `0`, every positive multiple of `locate_sample_rate`, and suffix position `n` are sampled; when `n` is not a multiple of the rate, `n` is an additional tail sample.

The companion extractor enumerates the sealed row-order samples with `select1(ordinal)` and maps each stored suffix position into deterministic position order. Its `sample_rows_by_position_` vector therefore satisfies:

`sample_rows_by_position_[slot(q)] = conceptual row whose suffix position is q`.

For periodic `q`, `slot(q) = q / rate`. When `n` is a non-periodic tail sample, it occupies the final extra slot.

Constructor checks reject missing, duplicate, non-periodic, out-of-range, or cardinality-inconsistent sample state rather than silently constructing an invalid inverse.

## Local extraction invariant

For a non-empty requested range `[begin,end)`, choose the smallest sampled suffix position `q >= end`:

- if `end` is periodic, `q=end`;
- otherwise use the next periodic suffix position when it is at most `n`;
- if no periodic position remains, use the mandatory tail sample `q=n`.

The conceptual row for suffix position `q` is obtained from the inverse sample table. For every suffix position `p>0`, the BWT byte at its row is `text[p-1]` and `LF` moves to the row for suffix position `p-1`. The extractor therefore repeats:

1. read the current BWT byte;
2. decrement the represented suffix position;
3. store the byte iff the new position is inside `[begin,end)`;
4. apply LF.

After exactly `q-begin` transitions, the requested range has been filled in source order. Bytes traversed in `[end,q)` are intentionally discarded setup work.

The old full-reconstruction API and the new sampled extractor share the same sealed BWT/LF state but have independent explicit cost surfaces.

## Deterministic LF-step bound

The anchor is at most one sample period after `end`:

`0 <= q-end <= rate-1`.

Hence for every non-empty range:

`lf_steps = q-begin = (end-begin) + (q-end)`

and therefore

`end-begin <= lf_steps <= (end-begin) + rate - 1`.

Production checks this bound at runtime. With sample rate one, extraction performs exactly one LF transition per returned byte. Empty ranges perform zero LF transitions.

## Space boundary

The extractor adds exactly one `size_t` row identifier per sealed periodic sample. `inverse_sample_payload_bytes()` reports this logical payload and excludes vector object, allocator, and spare-capacity overhead. The existing packed row-membership and row-order suffix-position sample payloads are reused unchanged.

For text length `n` and sample rate `r`, the inverse table is `O(n/r + 1)` entries. Output storage is exactly `O(end-begin)` bytes.

## Verification

Deterministic coverage checks:

- empty text and empty ranges;
- classic `banana` ranges;
- sample rate one, where LF steps equal the requested length exactly;
- a non-periodic final suffix position requiring the mandatory `n` tail sample;
- arbitrary embedded NUL/high bytes;
- sample rates larger than the text;
- inverse sample count/payload agreement with the sealed Phase-20 sample set;
- invalid and reversed ranges;
- a short range whose measured LF work is strictly below the sealed Phase-29 full-reconstruction baseline.

Randomized differential verification builds an independent source string, constructs the BWT index and companion extractor, and compares every returned byte string directly with `std::string::substr`. The test also checks the LF-step inequality above and sample-count/payload invariants across varied text lengths and locate sample rates. It does not reconstruct expected bytes through BWT/LF logic.

A separate algorithm-level property simulation was used while designing the slice to exercise random byte strings, sample rates, and ranges before repository integration. The authoritative integration evidence remains the repository's strict GCC, strict Clang, and ASan+UBSan CI on the exact candidate and merged `main`.

## Complexity and claim boundary

Extractor construction is `O(S)` time and `O(S)` extra resident storage for `S` periodic samples. A range of length `L` performs `O(L+r)` LF/rank-access work in the conservative direct bound, with the exact transition count exposed in the result, and allocates `O(L)` output storage.

This is a bounded local-extraction improvement over Phase 29's unconditional `n`-step reconstruction for a slice. It is not a universal speedup: a very large sample rate can make a short range walk close to the entire text, and full-range extraction still has linear work. No compressed-optimal random-access, r-index extraction, or asymptotically optimal text-access claim is implied.

## Phase boundary

Phase 31 changes the extraction substrate only. It deliberately does not modify Phase-30 seed-and-verify search in the same PR. After exact candidate CI, merged-main CI, and a clean Phase-31 audit, the repository may separately evaluate whether bounded candidate windows should replace Phase-30's one full reconstruction.
