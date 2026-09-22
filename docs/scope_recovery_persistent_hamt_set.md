# Scope recovery: persistent HAMT uint64 set

## Coverage decision

After the exact static range-mode index merged as
`main@a2663425fa92c7d7b45dec6ef5d2c6f1d344d3e0`, a fresh live audit found
zero open pull requests and zero open issues.

A first candidate, subarray predecessor/successor, was implemented only on an
unpublished branch and then rejected during the pre-PR architecture review
because its canonical-segment-node plus sorted-catalog proof model overlapped too
closely with the repository's existing orthogonal range tree. That branch was
force-reset to current `main` before selecting this slice.

The subsequent live audit found no HAMT / hash-array-mapped-trie implementation,
branch, or pull-request history.

Nearby hash capabilities are materially different:

- FKS is static perfect hashing;
- cuckoo and Robin Hood sets use flat mutable hash-table layouts;
- quotient/XOR filters are approximate membership structures;
- crit-bit is an ordered byte-string trie.

Nearby persistent capabilities such as the persistent segment tree, finger tree,
rope, and Hood-Melville queue do not provide hash-trie membership.

This slice therefore changes both representation and proof model: immutable
bitmap-radix nodes, deterministic 64-bit hash permutation, and path-copy
structural sharing.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen historical compiler/backend
frontier and stale ROADMAP presentation are not resumed.

## Public contract

`PersistentHamtSet64` stores exact `uint64_t` keys.

- the default value is the empty set;
- `contains(key)` performs exact membership;
- `insert(key)` returns a new immutable set version containing `key`;
- inserting an existing key returns an equivalent unchanged version;
- `erase(key)` returns a new immutable set version without `key`;
- erasing an absent key returns an equivalent unchanged version;
- every older version remains usable after arbitrary later branching updates;
- `size()` counts exact set cardinality;
- `reachable_node_count()` is a diagnostic over the selected version only;
- `valid_structure()` is an intentionally expensive structural replay.

The implementation is a set, not a map or multiset. Duplicate insertions never
increase cardinality.

## Deterministic hash permutation

The trie does not rely on a probabilistic no-collision assumption.

The internal 64-bit mixer is the composition of:

1. xor with a right shift;
2. multiplication modulo `2^64` by an odd constant;
3. another xor-right-shift;
4. multiplication by another odd constant;
5. a final xor-right-shift.

An xor-right-shift map on a fixed-width word is invertible because bits can be
recovered from most-significant to least-significant positions. Multiplication
by an odd number is invertible modulo `2^64`.

Every component is therefore a bijection on the 64-bit word domain, and their
composition is also a bijection. Distinct keys have distinct mixed hashes.

Consequently this uint64-specialized HAMT does not need an auxiliary full-hash
collision bucket.

## Radix / bitmap invariant

Five hash bits are consumed per level.

Twelve levels consume bits 0..59; the thirteenth level consumes the remaining
four bits 60..63. Thus every pair of distinct 64-bit hashes differs at or before
level 12, and trie descent needs at most thirteen radix decisions.

A branch stores:

- a 32-bit bitmap of occupied radix slots;
- a compact child vector containing only occupied slots;
- children in increasing bitmap-slot order.

For radix bit `b`, the compact child index is the population count of occupied
bits below `b`.

A branch with a single leaf child is non-canonical and is collapsed after erase.
A branch with one **branch** child may be necessary: it represents multiple keys
that share one or more leading radix chunks, so removing that depth would shift
the meaning of all deeper chunks.

## Persistent update invariant

Nodes are held through `shared_ptr<const Node>`; existing nodes are immutable.

For insertion or erase:

- untouched subtries are reused by pointer;
- each branch on the modified route receives a new compact child vector;
- only the changed child pointer is replaced/inserted/removed;
- a successful insert creates one new leaf plus only nodes on its radix route;
- erase copies only the affected route and performs canonical single-leaf
  collapse while unwinding;
- duplicate insert and absent erase return the existing set value.

Because no existing node is mutated, any version may be used as the base of a
later update without affecting sibling or ancestor versions.

## Structural replay

`valid_structure()` checks:

- empty-root/cardinality agreement;
- leaf hash equals the deterministic mixer applied to its key;
- leaves have no branch payload;
- branch depth never exceeds the 13-level hash width;
- bitmap population count equals compact child count;
- no child pointer is null;
- a single leaf child has been canonically collapsed;
- every leaf under bitmap slot `i` actually has radix chunk `i` at that
  depth;
- recursively counted leaves equal public set cardinality.

This replay is diagnostic evidence and is not used to answer membership queries.

## Complexity boundary

Let `w=64` and radix width `r=5`.

At most `ceil(w/r)=13` levels are visited. Bitmap rank uses one fixed-width
population count, and each branch stores at most 32 child pointers.

Therefore, for this fixed-width uint64 contract:

- membership visits at most 13 trie levels;
- successful insert/erase path-copies at most O(13) trie levels;
- each copied branch handles at most 32 child pointers;
- one changed version allocates O(13) structural nodes in the bounded-word
  model, plus one leaf for a new key;
- retained versions share all untouched subtries.

It is also valid to state the parameterized bound as `O(w/r)` levels with a
fixed maximum radix fanout `2^r`.

The diagnostic replay is deliberately excluded from these bounds.

## Independent verification

Committed deterministic tests cover:

- empty membership and structure;
- branching persistence from the same old version;
- duplicate insertion and absent erase;
- erase without modifying older versions;
- keys `0`, `UINT64_MAX`, and the top-bit boundary;
- dense insertion of 2,048 keys followed by erasing every odd key while keeping
  the original full version alive;
- structural replay before and after mass erasure.

The randomized oracle stores each logical version independently in
`std::set<uint64_t>`.

Across 1,400 fixed-seed branching updates, each new version chooses an arbitrary
older version as its base, performs insert or erase, and compares cardinality and
membership probes against the independent `std::set`. Structural replay is
performed periodically on both new and old versions rather than on every step,
so sanitizer evidence is not dominated by diagnostic work.

Supporting model-level evidence additionally:

- checked 200,000 sequential keys for duplicate mixed hashes in the model;
- executed 20,000 branching persistent insert/erase operations against immutable
  set snapshots with periodic full structural replay;
- found zero model mismatch.

Those model checks are supporting evidence only; they are not compiler,
sanitizer, or exhaustive 64-bit collision claims. The no-collision property used
by production follows from the permutation argument above.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

## Non-claims

This slice does not claim:

- arbitrary user-defined key or hash types;
- a Clojure-compatible HAMT serialization or node layout;
- transient/mutable batch updates;
- lock-free or wait-free concurrency;
- allocator or cache-locality optimality;
- benchmark-backed throughput;
- adversarial properties beyond the exact permutation/radix invariants stated
  here;
- cryptographic hashing or collision resistance.

The mixer is used for deterministic radix dispersion and bijection, not
cryptography.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/data_structures/persistent_hamt_set.hpp`;
- `tests/test_persistent_hamt_set_cases.hpp`;
- `docs/scope_recovery_persistent_hamt_set.md`.

Pre-PR hardening fixed:

- structural replay safety for an unexpected null child;
- randomized replay frequency so full-tree diagnostics remain evidence rather
  than dominating ASan runtime;
- the missing `<array>` test include.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Base: `a2663425fa92c7d7b45dec6ef5d2c6f1d344d3e0`.
