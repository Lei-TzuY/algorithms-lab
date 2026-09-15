# Scope recovery: arbitrary-byte crit-bit / Patricia string set

## Coverage decision

After min-plus matrix walk algebra reached merged-main green, a fresh default-branch,
pull-request-history, and branch-namespace audit found no Patricia trie, crit-bit tree,
or compressed-radix string-set capability. Several superficially attractive candidates
were deliberately rejected because their surfaces are already occupied or implemented:
BFPRT selection, Knuth/optimal-BST work, and Birkhoff-von-Neumann recovery all have
live branches, while CDQ dominance and convex Minkowski sum are already present.

This slice changes proof model again: it adds a path-compressed radix decision tree for
arbitrary byte strings rather than extending the adjacent tropical-matrix or occupied
recovery surfaces. Prospective work continues to follow
`docs/scope_recovery_after_phase69.md`; the historical compiler/backend roadmap remains
frozen.

## Production contract

`algorithms::data_structures::CritBitStringSet` is an owning unique-string set with:

- exact `insert`, `erase`, `contains`, `size`, and `empty` semantics;
- arbitrary bytes, including embedded NUL and bytes `0x80..0xFF`;
- explicit support for one key being a prefix of another, including the empty key;
- deterministic `values_unsigned_lexicographic()` output under unsigned-byte ordering;
- `internal_node_count()` and an expensive `valid_structure()` replay diagnostic;
- no library trie/radix-tree implementation underneath the studied structure.

Each real byte is encoded as a 9-bit symbol in `1..256`; symbol zero is a virtual
terminator. Symbols after the terminator are also zero. This separates a shorter prefix
key from every longer key without reserving any real byte value, so embedded NUL remains
ordinary data.

## Patricia / crit-bit invariant

Leaves own complete keys. Every internal node stores exactly one bit index. Along every
root-to-leaf path those indices increase strictly. Its zero and one subtrees contain keys
whose encoded bit at that index is respectively zero and one.

To insert a missing key, production first descends to the candidate leaf, computes the
**first** differing encoded bit, then walks from the root to the first node whose stored
bit is not smaller. A new internal node is spliced there. All allocations for the new
leaf/internal node complete before the existing subtree is moved, so allocation failure
cannot destroy the represented set before the commit point.

Erasing a non-root leaf removes its now-unary parent and splices the sibling directly
into the parent's former position. This preserves every remaining routing decision while
removing exactly one leaf and one internal node.

`valid_structure()` independently replays full-tree shape, strictly increasing bit
indices, the full-binary identity `internal = leaves - 1`, unique leaves, every leaf's
root routing, and canonical first-differing-bit placement between child subtrees.

These structural arguments are the proof obligations; finite testing is implementation
evidence rather than a replacement for the Patricia-trie correctness argument.

## Verification

Focused final bytes passed before upload:

- GCC C++20 repository strict warnings-as-errors: 5/5;
- Clang C++20 repository strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with fail-fast/leak detection: 5/5.

Source review caught and corrected one real exception-safety bug before upload: the first
prototype moved the existing subtree before allocating both replacement nodes. The final
candidate allocates first and mutates only after allocation succeeds.

Deterministic evidence covers empty and prefix keys, duplicate insertion, arbitrary NUL
and high-bit bytes, erase-induced path compression, reverse insertion order, exact
internal-node counts, and unsigned-byte ordered replay. A small exhaustive corpus inserts
and removes every length-0..2 string over `{0x00,0x80,0xFF}` while validating structure
after every operation.

Primary randomized evidence performs 600 fixed-seed mixed insert/erase/contains
operations on arbitrary byte strings of length 0..12. An independent
`std::set<std::string>` with an explicit unsigned-byte comparator supplies operation and
ordered-content results; production additionally replays its structural validator
periodically and at the end. The oracle does not use crit-bit routing or first-difference
logic.

## Complexity / non-claims

Let `H` be the number of internal nodes on the traversed route and `L` the relevant key
length. `contains`, `insert`, and `erase` use `O(H + L)` direct work in this concrete
arbitrary-length implementation; `H <= n-1` for `n` stored keys. The set stores exactly
`n` leaves and, for `n>0`, `n-1` internal nodes, plus the owned key bytes. The ordered
value snapshot adds sorting work. `valid_structure()` is deliberately expensive
verification, not a normal-operation complexity claim.

No prefix enumeration, longest-prefix matching, predecessor/successor API, Unicode
normalization, persistence, concurrency, lock-free behavior, allocation-failure test
injection, compressed key storage, or universal `O(L)` lookup claim is made.

## Scope

Exactly four paths differ from the green min-plus checkpoint:

- `include/algorithms/data_structures/crit_bit_string_set.hpp`;
- `tests/test_crit_bit_string_set_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_crit_bit_string_set.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen
compiler/backend, Birkhoff, BFPRT, Knuth, or temporary-file churn is included.
