# Scope recovery: exact byte-oriented LZW coding

## Coverage decision

A fresh post-Phase-69 live-state audit at `main@2098ee9b196e789c236ffb4f5527163b422a1863` found zero open PRs and zero open issues. Default-branch code, commit search, PR-history search, and branch-name search found no LZW / Lempel-Ziv-Welch implementation or occupied LZW branch.

The audit also found existing or occupied neighboring surfaces that are deliberately left untouched: sealed LZ78 dictionary parsing, sealed bounded-window LZ77 work, static arithmetic coding, the long-lived minimum-cycle-basis branch, and the diverged exact-treewidth branch. This slice therefore adds a different adaptive dictionary proof model instead of competing with those surfaces or resuming the frozen compiler/backend roadmap.

The prospective authority remains `docs/scope_recovery_after_phase69.md`.

## Production contract

`lzw_encode_bytes` and `lzw_decode_bytes` implement classical byte-oriented LZW over raw codewords.

- codes `0..255` are the fixed initial dictionary of literal bytes;
- arbitrary bytes, including `0x00`, `0x80`, and `0xff`, are ordinary symbols;
- empty input maps to an empty code stream and vice versa;
- the encoder greedily extends the current phrase while a `(prefix_code, next_byte)` transition exists;
- when that transition is absent, the current code is emitted and the new phrase is assigned the next dictionary code;
- the decoder stores each learned phrase compactly as `(parent_code, suffix_byte)` and reconstructs phrases by following backward parent links;
- the standard next-code `KwKwK` case is handled explicitly as `previous_phrase + first(previous_phrase)`;
- malformed streams whose first code is non-literal or whose later code skips beyond the one legal next-code case are rejected;
- codewords use `uint32_t`; if that code space were ever exhausted, dictionary growth freezes symmetrically in encoder and decoder.

The public surface deliberately returns codewords rather than a packed bitstream.

## Correctness obligation

At every encoder step, `current` names exactly the longest dictionary phrase matching the unread prefix. A present transition extends that phrase by one byte. A missing transition proves the longer phrase is not yet in the dictionary, so emitting `current` is the greedy LZW choice and inserting `current + next_byte` creates exactly the next phrase required by the standard recurrence.

For decoding, every ordinary learned entry points strictly backward to an already-defined prefix code and appends one byte, so phrase reconstruction terminates. For an incoming code equal to the next dictionary index, the only phrase the encoder could just have created but the decoder has not yet materialized is `previous_phrase + first(previous_phrase)`; this is the classical `KwKwK` case. Induction over the code stream therefore keeps encoder and decoder dictionaries synchronized.

## Independent verification

Before upload, the exact candidate header and recovery tests were compiled and executed locally with the repository warning profile under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- GCC ASan + UBSan with fail-fast behavior.

Deterministic coverage includes:

- empty input;
- the canonical `TOBEORNOTTOBEORTOBEORNOT` code sequence;
- the `ABABABA` / `KwKwK` decoder case;
- repeated `0x00`, `0x80`, and `0xff` bytes;
- malformed first-code and skip-ahead rejection;
- a 4096-byte repeated-symbol phrase chain.

The primary randomized oracle is structurally independent of the production transition map. It stores complete dictionary strings, linearly scans every known phrase at each input boundary, and chooses the longest matching phrase. Across 2,500 fixed-seed arbitrary-byte inputs of length `0..96`, the complete production code vector must match that oracle exactly, decoding must reproduce the original bytes, and repeated encoding must be deterministic.

## Complexity and non-claims

With `D` dictionary entries and input length `n`, the ordered transition map gives a conservative `O(n log D)` encoder lookup bound with `O(D)` dictionary storage. Decoder work is proportional to the bytes it reconstructs and emits, plus dictionary bookkeeping, with `O(D)` dictionary state and one current-phrase scratch buffer.

This slice makes no entropy bound, compression-ratio guarantee, optimal parsing claim, bit-packing claim, adaptive-width/reset policy claim, GIF/TIFF/file-format compatibility claim, or benchmark-backed performance claim.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/coding/lzw.hpp`;
- `tests/test_lzw_cases.hpp`;
- `docs/scope_recovery_lzw.md`.

The sealed isolated recovery-test architecture auto-enrolls the case header, so no CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen compiler/backend, or temporary-file change is required.

Base: `2098ee9b196e789c236ffb4f5527163b422a1863`.
