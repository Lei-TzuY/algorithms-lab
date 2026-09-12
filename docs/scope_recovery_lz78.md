# Scope recovery: exact LZ78 byte dictionary parsing

## Coverage decision

After the vertex-biconnected block-cut checkpoint, a fresh live default-branch,
pull-request-history, and branch audit found no LZ77, LZ78, LZW, or related
dictionary-parsing compression capability in `algorithms-lab`. The repository
already contains deterministic Huffman byte prefix coding and extensive BWT text
indexing, but neither models online phrase-dictionary growth.

This recovery slice therefore changes proof model again rather than extending
the recent graph-decomposition streak. It adds exact LZ78 parsing and decoding
over arbitrary bytes, with explicit terminal-phrase semantics and an independent
naive dictionary parser as the primary encoding oracle.

## Public contract

`lz78_encode(input)` treats dictionary index 0 as the empty phrase. At each input
boundary it consumes the longest phrase already present in the dictionary.

- If another byte remains, it emits `(prefix_index, next_byte)` and inserts the
  new phrase `dictionary[prefix_index] + next_byte` at the next dictionary index.
- If the input ends exactly after an already-known non-empty phrase, it emits one
  final terminal codeword `(prefix_index, nullopt)` and does not add a dictionary
  entry.
- Empty input produces an empty stream.
- Bytes are interpreted as `uint8_t`, so embedded NULs and high-bit bytes are
  ordinary symbols rather than terminators or signed characters.

`lz78_decode(codewords)` accepts prefix-index streams whose references point only
to dictionary entries created by earlier non-terminal codewords. A terminal
codeword must be last and must reference a non-empty phrase. The decoder checks
those structural safety conditions but does not claim to reject every
non-canonical stream that happens to decode successfully.

## Trie and decoder invariants

The encoder keeps one sparse trie node per dictionary phrase. Each node's
`phrase_index` names exactly the byte string on the root-to-node path. Walking
transitions from the root therefore finds an existing prefix of the remaining
input. The walk stops only at the first absent transition or at end of input, so
it is exactly the longest known phrase. An absent transition creates one new
phrase and one new trie edge; canonical LZ78 construction never inserts the same
`(prefix, byte)` transition twice.

The decoder deliberately does not store complete phrase strings. Dictionary
entry `i > 0` stores only `(parent_index, byte)`, with `parent_index < i` because
the reference existed before entry `i` was created. Following parent indices
therefore terminates at dictionary entry 0 and reconstructs that phrase in
reverse. Inductively, after every non-terminal codeword encoder and decoder
assign the same phrase text to the same next dictionary index. Concatenating the
reconstructed emitted phrases therefore reproduces the original input; a final
terminal codeword appends an already-known suffix without changing dictionary
numbering.

## Independent differential verification

The primary randomized encoding oracle intentionally does not use a trie. It
stores complete dictionary strings and, at every phrase boundary, scans every
known phrase to find the longest one matching the remaining input. That slow
formulation is structurally independent from production transition walking.

The fixed-seed corpus checks 2,500 arbitrary-byte strings of length `0..96`.
Every production codeword vector must equal the naive parser exactly, and every
production stream must decode byte-for-byte to the original input. Deterministic
coverage additionally locks:

- empty input and empty stream semantics;
- a terminal existing-phrase case (`aaaaa`);
- a classic repeated-prefix dictionary-growth example;
- embedded `0x00`, `0x80`, and `0xff` bytes; and
- future-prefix, root-terminal, and non-final-terminal rejection.

Focused strict GCC, strict Clang, and actual GCC ASan+UBSan execution pass the
repo-native five-test candidate before upload.

## Complexity and non-claims

Let `n` be the input byte length, `P` the number of created phrases, and
`Sigma = 256` the fixed byte alphabet. Encoder trie transitions are stored
sparsely; each transition lookup scans at most `Sigma` outgoing labels. The
baseline is therefore `O(Sigma * n)` time, which is `O(n)` for the fixed byte
alphabet, and `O(P)` resident trie nodes/edges plus returned codewords.

Decoding takes `O(output_bytes + codewords)` time because each emitted phrase is
reconstructed only while its bytes are appended, with `O(P + L)` auxiliary
storage for parent/byte dictionary entries and a scratch buffer of maximum phrase
length `L`, excluding returned output.

This slice makes no entropy bound, compression-ratio, bit-packing, canonical-file-
format, LZ77 sliding-window, LZW code-width, or universal-compression claim. The
returned representation is an educational exact phrase stream, not a compressed
container format.
