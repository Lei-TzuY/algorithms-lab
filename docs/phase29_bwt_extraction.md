# Phase 29 — BWT Text Reconstruction and Bounded Extraction

## Scope

Phase 29 adds an exact source-recovery surface to the resident `BwtByteIndex`. The constructor still consumes a `string_view` only while building the index; no source-text copy is added to resident state. `reconstruct_text()` recovers the constructor bytes from the conceptual-sentinel BWT and LF mapping, and `extract_text(begin,end)` returns a validated half-open slice.

This first baseline deliberately reconstructs the complete text before slicing. It is a correctness substrate for a future exact seed-and-verify architecture, not a compressed random-access or optimal extraction claim.

## Reconstruction invariant

Construction conceptually orders the empty suffix at row zero and the ordinary suffixes after it. Let the suffix position represented by the current row be `p`.

- row zero represents `p = n`;
- for every non-sentinel row with `p > 0`, its BWT byte is exactly `text[p-1]`;
- `LF(row)` represents suffix position `p-1`;
- the row representing suffix position zero is the explicit conceptual-sentinel row and has no resident BWT byte.

Starting at row zero, Phase 29 therefore reads one BWT byte and then applies LF exactly `n` times. The bytes appear as `text[n-1], text[n-2], ..., text[0]`, so production writes them into the result from right to left. After exactly `n` transitions the walk must reach `sentinel_row_`; reaching it early or failing to reach it is a structural logic error.

The recurrence uses only the sealed resident BWT access/rank state and LF mapping. Periodic locate samples and run-boundary suffix-position samples are irrelevant to reconstruction correctness, and no retained source string is consulted.

## Extraction contract

`extract_text(begin,end)` requires `0 <= begin <= end <= text_size()`. Invalid ranges throw `std::out_of_range`.

The baseline calls complete reconstruction and then slices `[begin,end)`. Its result diagnostics expose both the LF-step count and the number of reconstructed bytes. Consequently even a short slice currently performs exactly `n` LF steps and reconstructs `n` bytes. This cost is intentional and observable rather than hidden.

## Complexity boundary

For text length `n`, complete reconstruction performs exactly `n` LF transitions and stores `n` result bytes. Under the sealed run-length BWT occurrence representation, each LF step inherits the current rank/access cost; Phase 29 does not relabel that as constant time. The baseline extraction additionally copies the requested output bytes after the same full reconstruction.

Query workspace is `O(n)` plus output. No compressed-optimal random access, r-index extraction bound, source-compressed construction, seed-and-extend, or polynomial-time approximate-search claim is made.

## Verification

Deterministic tests cover empty text, `banana`, empty/full/interior half-open slices, invalid ranges, arbitrary `0x00`/`0x80`/`0xff` bytes, every byte value `0..255`, repeated calls, and deliberately different periodic locate sample rates.

A fixed-seed randomized differential corpus constructs 400 arbitrary-byte texts of length `0..128`. Every full reconstruction is compared byte-for-byte with the constructor input, and 40 independently chosen valid slices per text are compared with direct `std::string::substr`. Every successful call also checks the baseline accounting identity `lf_steps == reconstructed_bytes == text_size()`.

An algorithm-focused pre-upload prototype exercised the same LF reconstruction recurrence under strict GCC, strict Clang, and real GCC ASan+UBSan on an earlier sealed BWT occurrence backend. Exact current-backend integration remains gated by the repository's full three-job remote CI; the prototype is not presented as a substitute for that gate.

## Phase boundary

This implementation establishes only reconstruction/extraction substrate. It does not implement exact seed-and-verify approximate search. Phase 29 remains active until exact candidate CI, clean integration, merged-main CI, and a post-merge sealing audit succeed.
