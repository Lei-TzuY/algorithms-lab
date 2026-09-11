# Scope recovery: online palindromic tree byte index

## Coverage decision

A fresh live-code and recent-commit audit after the exact KD-tree and Fibonacci-
heap recovery checkpoints found no Eertree, palindromic-tree, or Manacher
capability in the repository. Existing string structures cover different state:
KMP/Z reuse prefix borders, Aho-Corasick indexes a supplied pattern set, the
suffix automaton indexes substring end-position equivalence, and the BWT family
indexes text intervals. None maintains one stable node per distinct palindrome
with online longest-palindromic-suffix links and occurrence propagation.

This recovery slice deliberately changes proof model again after the recent
Fibonacci-heap cascading-cut, KD-tree spatial-partition, B-tree occupancy-repair,
and van-Emde-Boas universe-decomposition checkpoints. The frozen Phase-45--69
compiler/backend surface remains untouched. Historical active headings in
`ROADMAP.md` are not prospective authority; `docs/scope_recovery_after_phase69.md`
plus a fresh coverage audit remain the governing boundary.

## Production contract

`algorithms::strings::PalindromicTree` is an online arbitrary-byte Eertree.

- Two internal roots represent lengths `-1` and `0`.
- `append(byte)` extends the indexed text and returns the stable node id of the
  longest palindromic suffix after that append.
- Every distinct non-empty palindrome is represented by exactly one stable node.
- `longest_suffix_length()` reports the current longest palindromic suffix.
- `palindromes()` returns stable creation-order records containing node id,
  length, first occurrence interval, exact total occurrence multiplicity, and
  the length of the longest proper palindromic suffix.
- Bytes are unsigned symbols in `[0,255]`; embedded NUL and high-bit bytes have
  no special meaning.
- `valid_structure()` is an intentionally expensive teaching/test diagnostic
  that replays representation invariants rather than a normal query primitive.

No standard-library string index, palindrome algorithm, or third-party Eertree
is used for production behavior. Ordered containers appear only in the
independent test oracle.

## Structural invariants and proof obligations

For an append at position `i`, production follows suffix links from the previous
longest palindromic suffix until it finds the longest node whose occurrence can
be wrapped by the new byte. A transition on that byte then either reuses an
existing palindrome node or creates exactly one new node.

For every non-root node:

- its stored first-occurrence slice is a palindrome of the stored length;
- the node is unique for that byte string;
- its suffix link targets the longest *proper* palindromic suffix;
- a transition `P --c--> Q` satisfies `Q = c P c` (with the odd root producing
  length-one palindromes), so transition length is exactly parent length plus two;
- node ids are stable creation ids and every transition targets a later-created
  node.

Only the node that is the longest palindromic suffix at an append increments its
raw terminal counter. Every occurrence of a longer palindrome also contains its
suffix-link palindrome ending at the same position, so propagating counters from
newer/longer nodes through suffix links yields exact total occurrence counts.
`palindromes()` performs this propagation on a copy and therefore does not mutate
online state.

The current longest-suffix pointer must equal the longest stored palindrome whose
byte string is a suffix of the complete indexed text. `valid_structure()`
recomputes this property, suffix-link maximality, transition wrapping, unique
palindrome content, and first-occurrence validity.

## Mutation and representability boundary

Node lengths are stored as `std::ptrdiff_t`; appending beyond its positive range
is rejected explicitly. Before a new palindrome node is committed, production
computes its parent and suffix link and, when necessary, reserves node capacity
with geometric growth. Only after all potentially throwing node-allocation work
succeeds does it append the input byte and publish the node/transition. Thus an
allocation failure cannot leave the logical text one byte ahead of the Eertree
node state. Existing-transition appends rely on `std::vector<uint8_t>::push_back`
strong exception safety before incrementing the non-throwing counter/state.

The implementation makes no allocator, concurrency, lock-free, persistence, or
transactional recovery claim beyond this single-object append ordering.

## Complexity boundary

The alphabet is the fixed byte domain, so each node owns 256 direct transition
slots and transition lookup is constant time with a large fixed storage factor.
If `P` distinct non-empty palindromes have been created, resident state is
`O(256 P + n)`, conventionally `O(P+n)` for the fixed alphabet.

Each append follows a suffix-link chain until an extendable palindrome is found;
this implementation therefore claims the conservative code-local bound `O(n)`
for one append and `O(n^2)` for constructing an `n`-byte text, rather than
asserting a tighter classical Eertree bound without tying that proof to this
exact implementation. `palindromes()` is `O(P)` after output allocation.

`valid_structure()` is intentionally much more expensive: it compares stored
palindrome contents and recomputes longest proper suffix relationships, so its
teaching/diagnostic bound can reach `O(P^2 n + 256P)`. It is not included in the
online-operation complexity claim.

## Verification

Focused pre-upload verification passes under repository-equivalent settings:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- GCC ASan+UBSan with frame pointers enabled.

Deterministic cases cover the empty text, singleton, repeated bytes, nested odd
palindromes (`ababa`), all-distinct bytes, embedded NUL/high-bit bytes, online
longest-suffix node ids, repeated-node reuse, and known multiplicities.

The primary randomized differential corpus uses 700 fixed-seed arbitrary-byte
strings of length `0..24`. The independent oracle enumerates every substring,
tests palindromicity directly, and records the distinct palindrome set, exact
multiplicity, and earliest occurrence. It independently scans each palindrome to
find its longest proper palindromic suffix and independently scans the complete
text for the longest palindromic suffix. Every production record and structural
diagnostic is checked against those results.

This bounded corpus is executable evidence, not the source of the Eertree
correctness proof or a performance benchmark.

## Non-claims

This slice does not implement Manacher radii, palindromic factorization,
palindrome partition DP, dynamic deletion, compressed transitions, suffix trees,
or a universal fastest-palindrome-search claim. A subsequent recovery slice must
again pass a fresh coverage audit and change algorithmic/data-structural depth
rather than merely wrapping this index with adjacent palindrome helpers.
