# Scope recovery: x-fast trie predecessor set

## Coverage decision

Fresh live audit at `main@50ce468563da4a014026891772919339badc6d74` found no x-fast/y-fast implementation in default-branch code, pull-request history, or branch namespace. The sealed bounded-universe van Emde Boas recovery (#177) explicitly excludes sparse-vEB, y-fast `O(n)`-space, and hashing. This slice recovers the x-fast prefix-index substrate without falsely claiming the stronger y-fast space bound.

The post-Phase-69 recovery authority remains prospective. Historical compiler/backend work stays frozen, and this checkpoint does not modify the historical ROADMAP or recovery-authority document.

## Production contract

`XFastTrieSet(universe_bits)` supports a unique unsigned-key set over `[0,2^w)` for `1 <= w <= 64`:

- exact `insert`, `erase`, and `contains`;
- exact minimum / maximum;
- strict predecessor / successor;
- full `uint64_t` universe when `w=64`;
- out-of-universe keys reject for narrower universes;
- an expensive `valid_structure()` diagnostic replays prefix summaries and leaf-link ordering.

Each existing binary prefix is stored in a per-level hash dictionary with subtree `count/minimum/maximum`. Exact leaves also form an explicit ordered doubly linked list. No caller-visible result depends on hash-table iteration order.

## X-fast invariant and proof boundary

For every represented key and every prefix length `0..w`, the corresponding prefix exists. Conversely, a stored prefix exists iff at least one represented key has that prefix. Its summary stores the exact represented-key count and subtree extrema.

Prefix existence is monotone in prefix length, so predecessor/successor binary-search the longest prefix of the query key. If the next query-bit child is absent, the opposite sibling gives the nearest subtree extremum on one side; when that side lies outside the matched prefix subtree, the explicit leaf predecessor/successor link supplies the answer. Insertions update every prefix summary and splice one leaf into the order; deletions repair summaries bottom-up from their surviving children.

Correctness relies on the classical x-fast trie longest-prefix argument plus these replayable summary/link invariants. Tests are implementation evidence, not a replacement for that argument.

## Complexity and claim boundary

Let `U=2^w` and `n` be the stored-key count. The direct implementation stores at most one prefix per stored key per level, so resident state is `O(n log U)`.

- predecessor/successor use `O(log w) = O(log log U)` prefix-dictionary probes;
- insert/erase touch `w+1 = O(log U)` prefix dictionaries;
- contains/min/max use expected constant dictionary work after validation.

These are **average/expected hash-dictionary bounds** under the standard `std::unordered_map` model. This implementation makes no adversarial worst-case hash bound, perfect-hashing claim, deterministic time guarantee, or cryptographic-hash claim. Hash collisions can degrade an individual dictionary operation.

This is deliberately **not** a y-fast trie: no representative sampling/bucketing is implemented, and no `O(n)`-space or expected `O(log log U)` update claim is made. A later y-fast layer would need its own sampling/rebalancing/replay contract rather than being inferred from this checkpoint.

## Independent verification

Focused final candidate passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 5/5;
- Clang C++20 repository strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with fail-fast/leak detection: 5/5.

Evidence includes invalid universe widths/keys, empty and duplicate semantics, strict predecessor/successor boundaries, full `uint64_t` endpoints, prefix-summary repair after deleting subtree extrema, monotone insertion/complete clearing, and exact structural replay.

The primary behavioral oracle is independent `std::set<uint64_t>`: 30,000 fixed-seed mixed insert/erase/contains/predecessor/successor operations over a 16-bit universe compare every operation result, exact size, and extrema. A second 15,000-operation corpus uses raw full-width `uint64_t` keys against the same independent oracle to exercise 64-level prefix arithmetic. `valid_structure()` periodically reconstructs every stored prefix summary from the leaf set and replays the complete leaf-link chain.

## Scope

Exactly four paths change:

- `include/algorithms/data_structures/x_fast_trie_set.hpp`;
- `tests/test_x_fast_trie_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof/coverage document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or unrelated recovery surface changes.
