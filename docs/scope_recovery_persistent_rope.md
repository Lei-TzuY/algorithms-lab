# Scope recovery: persistent balanced byte rope

## Coverage decision

A fresh post-Phase-69 live audit after the Robin Hood open-addressing checkpoint found no persistent rope / persistent sequence-tree implementation in default-branch code, pull-request history, or the branch namespace. Nearby ordered-set, predecessor, heap, trie, and hashing families are already heavily represented; this slice instead changes the state model to an immutable sequence with structural sharing.

The historically drifted compiler/backend roadmap remains non-authoritative. Prospective work follows `docs/scope_recovery_after_phase69.md` plus the live coverage audit.

## Production contract

`algorithms::data_structures::PersistentByteRope` is an immutable arbitrary-byte sequence.

- construction owns a snapshot of the input bytes;
- empty, size, height, leaf-count, indexed byte access, and exact flattening;
- persistent `split`, `concat`, `insert`, `erase`, and `slice` return new ropes without mutating any existing version;
- leaves contain 1..64 bytes; internal nodes own no byte payload;
- every internal node caches exact logical byte length, AVL height, and logical leaf count;
- unchanged subtrees are shared through immutable node ownership;
- diagnostics replay the full AVL/cached-size representation and can count unique physical nodes or nodes shared between two versions.

Arbitrary bytes include embedded NUL and high-bit values. Ranges are zero-based, half-open, and bounds errors throw `std::out_of_range`.

## Structural invariant / proof boundary

Every internal node concatenates the complete byte sequence of its left child before its right child and satisfies an AVL height difference of at most one. Leaves are bounded to at most 64 bytes. Cached length and leaf count are exact sums of the children.

`join(a,b)` preserves inorder concatenation. If the heights differ by more than one, it descends only the taller spine, recursively joins there, and applies an immutable AVL single/double rotation on the rebuilt path. `split(node,k)` follows cached left lengths; untouched sibling subtrees are reused, while splitting inside one leaf copies at most 64 bytes. Insert/erase/slice are compositions of those two primitives.

Because nodes are immutable, allocation or validation failure while constructing a new version cannot partially mutate an older version. Concatenating a rope with itself is legal: both logical child occurrences may reference the same physical immutable subtree, so the representation is intentionally a DAG across/within versions rather than requiring deep copies.

The AVL-height theorem and persistence-by-path-copying argument are proof obligations; tests are implementation evidence rather than theorem substitutes.

## Verification

Focused final candidate passed repository-equivalent C++20 strict warnings under GCC and Clang plus actual GCC ASan+UBSan, 4/4 each.

Committed evidence covers:
- empty/bounds semantics and arbitrary `0x00/0x80/0xff` bytes;
- exact leaf-boundary and mid-rope split/concat replay;
- observable subtree sharing across split, slice, insertion, and self-concat;
- 6,000 append operations followed by 1,200 middle erasures with repeated AVL validation;
- 5,000 fixed-seed persistent operations where every new version is derived from an arbitrary older version by insert/erase/slice/concat/split+rejoin and is compared byte-for-byte with an independent `std::string` oracle;
- old randomly selected versions are rechecked throughout, making accidental mutation of historical snapshots observable.

An additional local stress pass ran 12 deterministic seeds times 20,000 mixed operations with continuous string-oracle and structure replay; it is supplementary evidence and is not added to the already-large full suite.

The standard string is only a test oracle; it is not used by the rope algorithms except as owned leaf payload.

## Complexity / non-claims

Let `L` be the number of logical leaf occurrences in a version and `n` its byte length. AVL balance gives height `O(log(L+1))`, with `L <= n` for non-empty ropes.

- `at`: `O(log(L+1))`;
- `concat`: `O(|h1-h2|+1)` path work;
- `split`, `erase`, and `slice`: `O(log(L+1))` tree work plus at most constant 64-byte boundary-leaf copying;
- inserting `k` bytes: `O(k + log(L+1))` direct work for chunk construction and persistent tree surgery;
- flattening: `O(n)`;
- structural/sharing diagnostics are intentionally linear in visited logical or unique physical nodes and are excluded from operation bounds.

Edits allocate only rebuilt search/join paths plus new or split boundary leaves; unchanged subtrees remain shared while referenced by old/new versions.

No mutable iterator API, leaf-coalescing guarantee, thread-safety guarantee, allocator-failure no-throw guarantee, lock-free behavior, finger-tree bound, implicit-treap/randomized balancing, text-editor benchmark, Unicode semantics, or library-replacement claim is implied.
