# Scope recovery: deterministic Huffman byte prefix coding

## Coverage decision

Fresh live code and pull-request searches after the merged explicit-family Count-Min
checkpoint found no Huffman, prefix-code, or canonical-prefix-coding capability in
`algorithms-lab`.  This slice deliberately changes proof model again: it leaves
probabilistic streaming, online palindromic indexing, meldable heaps, spatial search,
and balanced search trees for deterministic greedy optimal prefix coding.  The frozen
Phase-45--69 compiler/backend surface remains untouched.

## Production contract

`algorithms::coding::HuffmanByteCodebook` accepts one non-negative `uint64_t`
frequency for each of the 256 byte values.

- zero-frequency symbols are absent from the codebook;
- an empty alphabet has an empty tree and can encode/decode only the empty stream;
- a one-symbol alphabet uses the explicit one-bit code `0` so concatenated encoded
  streams remain decodable without an external symbol count;
- with at least two symbols, the two least-weight subtrees are repeatedly removed from
  the sealed first-principles `BinaryHeap` and merged;
- ties are deterministic by `(subtree weight, smallest byte in the subtree, node id)`;
- left/right tree edges emit `0`/`1` respectively;
- `encode_bits` rejects bytes absent from the codebook;
- `decode_bits` rejects non-binary input and trailing partial codewords;
- aggregate subtree weights and the public weighted bit count are checked for
  `uint64_t` representability instead of wrapping.

The returned representation is a deterministic tree-derived Huffman code, not a claim
of canonical-Huffman bit assignment.  The optimization objective is weighted codeword
length; among multiple optimal trees the tie rule merely chooses one replayable tree.

## Greedy proof obligation

For a positive-frequency alphabet with at least two symbols, there exists an optimal
full binary prefix tree whose two least frequent symbols occupy sibling leaves at
maximum depth.  Contracting those siblings into one pseudo-symbol with combined weight
reduces the problem to a smaller optimal prefix-code instance.  Repeating this
contraction yields the Huffman greedy algorithm.  Expanding the contractions therefore
recovers an optimal weighted external path length.

The one-symbol convention is handled separately: the repository intentionally requires
a non-empty codeword and assigns length one.

This exchange/contraction theorem is the correctness obligation.  Randomized tests are
implementation evidence, not a replacement proof.

## Executable invariants

The intentionally expensive `valid_codebook()` diagnostic independently replays the
constructed state:

- every positive-frequency symbol appears in exactly one reachable leaf;
- every leaf weight equals its input frequency;
- every internal weight equals the exact sum of its two children and its stored minimum
  symbol equals the children's minimum;
- every public code consists only of binary digits, is prefix-free, and follows actual
  tree edges to the corresponding byte without hitting another leaf early;
- all allocated nodes are reachable exactly once from the root;
- replaying `frequency * code_length` equals the stored weighted bit count.

## Verification

Focused pre-upload evidence passed under repository-equivalent flags:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic cases cover the empty alphabet, one-symbol one-bit convention, arbitrary
bytes including `0x00`, `0x80`, and `0xff`, equal-weight ties, the classical
`45/13/12/16/9/5` example with optimal weighted cost `224`, encode/decode round trips,
invalid bits, trailing partial codewords, absent-symbol encoding, aggregate-frequency
overflow, and weighted-bit-count overflow.

The primary optimality oracle is deliberately not another Huffman merge.  For 700
fixed-seed alphabets with at most six positive-frequency symbols, tests enumerate every
full-binary-tree leaf-depth multiset and every assignment of the test weights to those
leaf depths, then compare the exact minimum weighted path length with production.  The
same trials replay prefix-freeness, stored weighted cost, and encoded payload round trips.

## Complexity and non-claims

The byte alphabet is fixed at at most 256 leaves.  For `s` positive-frequency symbols,
construction performs `s-1` heap merges in `O(s log s)` time and stores `O(s)` tree
nodes plus code strings.  Encoding costs `O(B)` in produced bits.  Decoding costs `O(B)`
in input bits.  `valid_codebook()` is intentionally diagnostic and may use quadratic
pairwise prefix checks over the fixed alphabet.

This recovery slice does not implement adaptive Huffman coding, length-limited Huffman,
canonical-code serialization, byte packing, entropy estimation, arithmetic coding, or a
compression file format.  No compression-ratio or information-theoretic optimality
claim beyond binary prefix-code weighted path length is implied.
