# Scope recovery — bounded sliding-window LZ77

## Coverage decision

This recovery slice adds exact greedy LZ77 parsing over arbitrary bytes after the
Euler-tour-tree dynamic-forest checkpoint. Live pull-request/history searches
find the existing LZ78 dictionary parser and static arithmetic coder but no LZ77
or sliding-window back-reference implementation. The LZ78 and arithmetic-coding
proof documents explicitly leave LZ77 outside their contracts. Occupied
fractional-cascading, minimum-cycle-basis, and general-graph-isomorphism
surfaces are untouched.

This deliberately changes proof model from the recent dynamic-forest work:
bounded historical references, overlapping copy semantics, and a replayable
byte stream rather than dynamic-tree split/merge invariants.

## Production contract

`lz77_encode(input, window_size, max_match_length)` returns canonical tokens:

- a literal has `distance = 0`, `length = 0`, and one byte;
- a match has no literal, `1 <= distance <= window_size`, and length at least 3;
- match length is capped by `max_match_length` and the remaining input;
- overlapping matches are valid, so a distance-one match may repeat one prior
  byte for many output positions;
- at every parse boundary the longest valid match wins; equal-length matches
  choose the nearest source (smallest distance);
- if no length-at-least-three match exists, one literal byte is emitted;
- zero window or maximum match length below three is rejected.

`lz77_decode(tokens)` accepts only those canonical literal/match shapes. A match
source must already begin inside the decoded prefix. Copying is byte-wise from
`output.size() - distance`, so the source advances together with the output and
therefore implements standard overlapping LZ77 references. Malformed shapes or
references before the decoded prefix are rejected.

Arbitrary bytes, including NUL and high-bit bytes, have ordinary unsigned-byte
semantics. Tokens are an educational logical representation; no bit packing or
file format is implied.

## Search invariant

Production pre-indexes every three-byte prefix into ordered input positions. At
parse position `p`, only indexed positions in `[p-window_size, p)` with the same
three-byte key are candidates. Every legal match has length at least three, so
this filter cannot exclude a legal match. Candidates are visited nearest first;
full byte comparison extends each candidate up to the configured limit.

For candidate distance `d`, copied byte `k` is compared with original byte
`input[p-d+k]`. When `k >= d`, that source lies inside the match currently being
produced. The decoder's byte-wise recurrence reads the same logical byte at
`output[p+k-d]`, so the encoder comparison exactly models overlapping-copy
semantics rather than requiring the entire source phrase to precede `p`.

Visiting candidates from nearest to farthest and replacing the incumbent only
for a strictly longer match implements the public nearest-source tie rule.

## Verification

Focused final candidate passes:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

Deterministic evidence covers empty input, pure literals, distance-one overlap,
explicit maximum-length clipping, nearest-source ties, arbitrary `0x00` /
`0x80` / `0xff` bytes, configuration validation, and malformed decoder tokens.

The primary randomized oracle is structurally independent of the production
three-byte catalog. For 3,000 fixed-seed byte strings of length 0..128, random
windows 1..32, and random match limits 3..32, the oracle scans every legal
back-reference distance directly and extends it byte by byte. The complete
production token sequence must equal this brute-force greedy parse exactly, and
every stream must decode byte-for-byte to its input.

## Complexity / non-claims

Let `n` be input length, `W` the window size, and `L` the maximum match length.
Building the ordered three-byte catalog costs `O(n log n)` time and `O(n)`
storage. A conservative bound for the direct candidate-extension loop is
`O(n*W*L)` across at most `n` parse boundaries, giving
`O(n log n + n*W*L)` time and `O(n)` auxiliary/output-index storage. Decoder
cost is linear in the decoded byte count plus token count.

No entropy bound, compression-ratio guarantee, optimal-parse claim, suffix-array
acceleration, hash-based match finder, LZW semantics, bit packing, streaming
bounded-memory encoder, or standardized archive/file format is claimed.
