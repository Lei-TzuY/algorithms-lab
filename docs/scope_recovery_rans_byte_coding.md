# Scope recovery: deterministic byte rANS coding

## Coverage decision

After exact ordered rooted-tree edit distance merged as
`main@e86726147076225171aadac9460fcf1dc7e084c4`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains deterministic Huffman byte coding, static
arithmetic byte coding, LZ77/LZ78/LZW dictionary coding, length-limited Huffman,
and deterministic pair-replacement grammar construction.

No rANS / Asymmetric Numeral Systems implementation, branch, or prior
implementation pull request was found.

This slice therefore adds a materially different coding state machine:
finite-state ANS renormalization with a power-of-two normalized probability
model. It does not reuse prefix-tree emission, arithmetic interval subdivision,
or dictionary parsing.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; frozen historical compiler/backend
work is untouched.

## Public block contract

`RansByteBlock` contains:

- a deterministic 256-entry normalized byte-frequency table;
- one 64-bit final ANS state;
- the byte renormalization stream;
- the exact decoded symbol count.

The public operations are:

- `rans_encode_bytes(input)`;
- `rans_decode_bytes(block)`.

The scale is fixed at 12 bits, so all non-empty normalized models sum exactly to
4096. The lower renormalization state is `2^31`.

An empty block is canonical only when:

- symbol count is zero;
- every model frequency is zero;
- state equals the lower bound;
- renormalization bytes are empty.

## Deterministic model normalization

Construction counts all 256 byte values.

For every observed symbol:

1. compute `floor(count * 4096 / input_size)`;
2. force the result to at least one;
3. retain the exact integer remainder from the division.

If the resulting total is below 4096, residual units are assigned in descending
remainder order, breaking equal remainders by smaller byte value.

If forcing rare symbols to frequency one makes the total exceed 4096, units are
removed only from symbols with frequency above one. Donors are selected
deterministically by:

1. smaller remainder;
2. larger current removable frequency;
3. smaller byte value.

Thus:

- every observed symbol has positive frequency;
- no unseen symbol is forced positive by normalization;
- the exact total is 4096;
- the same byte input produces the same normalized model.

The input-size guard rejects the theoretical `count * 4096` overflow boundary
before multiplication.

This is a deterministic apportionment policy. It is not claimed to minimize
coding redundancy.

## rANS state transition

For symbol frequency `f`, cumulative start `c`, and 12-bit scale:

`C(x,s) = floor(x/f) * 4096 + (x mod f) + c`.

Encoding processes input symbols in reverse order.

Before applying the state transition, low bytes are emitted while the current
state is at or above the symbol-specific renormalization limit:

`((L >> 12) << 8) * f`,

where `L = 2^31`.

Emitted low bytes are collected during reverse-symbol encoding and then reversed
once in the stored block. The decoder can therefore consume the renormalization
byte vector from left to right while reconstructing the original symbol order.

The chosen lower bound and scale keep valid encoder transitions well within the
64-bit state domain.

## Inverse decoder transition

For current state `x`, let:

`r = x mod 4096`.

The decoder lookup table maps every residue slot to the unique symbol whose
normalized cumulative interval contains that residue.

For decoded symbol frequency `f` and cumulative start `c`:

`D(x,s) = f * floor(x/4096) + (r-c)`.

After the inverse transition, bytes are consumed while the state is below
`L`:

`x = (x << 8) | next_byte`.

Malformed external blocks are rejected when:

- a non-empty model does not sum exactly to 4096;
- the initial state is below `L`;
- a decoder multiplication would overflow 64 bits;
- renormalization bytes are truncated;
- bytes remain after the declared symbol count;
- the final decoder state is not exactly `L`;
- the empty-block canonical representation is violated.

The terminal-state and byte-consumption checks prevent acceptance of alternate
trailing representations of the same decoded prefix.

## Verification evidence

Committed deterministic tests cover:

- canonical empty block;
- a 4096-byte single-symbol block, whose model frequency is exactly 4096 and
  whose state never changes from `L`;
- balanced `"abab"`, pinning frequencies and exact final state;
- a nontrivial manually specified `"banana"` block, pinning frequencies,
  final state, and the emitted renormalization byte before asking the decoder to
  reconstruct the string;
- all 256 byte values;
- a highly skewed block with 100,000 zero bytes plus every other byte once;
- embedded arbitrary binary data;
- 900 fixed-seed random blocks across alphabets of size 2, 7, 31, and 256;
- deterministic repeated encoding;
- malformed empty blocks;
- wrong model total;
- below-bound initial state;
- truncated renormalization stream;
- trailing renormalization byte;
- incorrect declared symbol count.

Before repository integration, an independent model-level implementation also
exercised empty blocks, single-symbol blocks, 100,000-symbol skewed blocks,
uniform 256-symbol blocks, and many random inputs with zero round-trip mismatch.
That model check is supporting evidence only; repository CI is the integration
gate.

## Complexity boundary

Let `N` be the input symbol count and `A=256` the fixed byte alphabet.

Model construction is `O(N + A^2)` under the bounded deterministic adjustment
loop; because `A` is fixed, this is linear in input size for the public byte
contract.

Encoding and decoding are linear in symbols plus emitted/consumed
renormalization bytes.

The decoder builds a fixed 4096-entry residue lookup table.

The block stores:

- 256 normalized frequencies;
- one 64-bit state;
- one symbol count;
- the renormalization byte vector.

This slice does not claim a serialized-space advantage because the in-memory
frequency table and metadata are deliberately explicit.

## Non-claims

This slice does not claim:

- a standardized rANS file/wire format;
- compatibility with external ANS libraries;
- adaptive or streaming probability updates;
- interleaved SIMD rANS states;
- optimal frequency normalization;
- entropy-optimal or arithmetic-coder-equivalent compressed size;
- bounded-memory streaming;
- compression-ratio or throughput benchmarks;
- cryptographic properties.

The fixed known vectors are regression anchors for this implementation's public
block representation, not an interoperability standard.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/coding/rans_byte.hpp`;
- `tests/test_rans_byte_cases.hpp`;
- `docs/scope_recovery_rans_byte_coding.md`.

Pre-PR hardening includes:

- explicit `<optional>` / `<utility>` production dependencies;
- explicit `<vector>` test dependency;
- a manually specified nontrivial decoder vector for `"banana"`.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `e86726147076225171aadac9460fcf1dc7e084c4`.
