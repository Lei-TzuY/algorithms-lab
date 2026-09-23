# Scope recovery: Roaring-style uint32 bitmap set

## Coverage decision

After the DFA-equivalence witness merged as
`main@cebf6850e0873e6d1aa5c0beb703265d5c9729da`, a fresh live audit found
zero open pull requests and zero open issues.

Searches across default-branch code, branch names, and pull-request history found
no Roaring bitmap or Roaring-style containerized uint32 set implementation.

The repository already contains packed rank/select, Elias-Fano, wavelet-matrix,
hash-set, persistent-tree, and packed-memory-array structures. This slice is
different in representation and mutation proof model: the 32-bit key space is
partitioned by high 16 bits, and each occupied partition dynamically switches
between a sparse sorted-array container and a dense fixed bitmap container.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; frozen historical compiler/backend
work is not resumed.

## Public contract

`RoaringStyleBitmap32` is an exact mutable set of `uint32_t` values.

It supports:

- `insert(value)`, returning whether the set changed;
- `erase(value)`, returning whether the set changed;
- `contains(value)`;
- exact total `size()`;
- `rank(value)`: number of stored values less than or equal to `value`;
- `select(index)`: zero-based kth-smallest value, or `nullopt` when out of
  range;
- container-count diagnostics;
- `valid_structure()` for expensive invariant replay.

Duplicate insertion and absent erasure are no-ops.

The class is intentionally named **Roaring-style**. It does not claim byte-for-
byte Roaring serialization compatibility.

## Two-level representation

A 32-bit value is split into:

- high 16 bits: container key;
- low 16 bits: position inside that container.

Containers are stored in strictly increasing high-key order.

Each container stores an exact cardinality and one of two payloads:

### Sparse array container

For cardinality at most 4096, lows are stored as a strictly increasing
`vector<uint16_t>`.

Membership uses binary search.

The 4096 threshold is also the payload-size crossover: 4096 two-byte lows use
8192 payload bytes, the same bit payload size as a complete 65,536-bit bitmap.

### Dense bitmap container

When inserting the 4097th distinct low, the container becomes 1024
`uint64_t` words, exactly 65,536 bits.

Membership is one word lookup and bit test after the high container is found.

When erasure reduces a bitmap container to 4096 values, it converts back to the
sorted sparse representation.

Run containers are not implemented in this slice.

## Transition invariants and exception boundary

Representation changes are prepared transactionally.

For sparse-to-dense transition:

1. the duplicate check and cardinality-capacity check happen first;
2. a complete replacement bitmap is built from the existing sparse values plus
   the new low;
3. only after construction succeeds is the payload replaced;
4. public cardinalities are then incremented.

For dense-to-sparse transition:

1. membership of the erased bit is established first;
2. a replacement sorted array excluding that low is fully built;
3. only after construction succeeds is the payload replaced;
4. public cardinalities are then decremented.

Thus allocation failure while constructing a replacement payload does not first
commit the logical insertion or erasure.

Ordinary sparse insert/erase and dense bit insert/erase update cardinality only
after the underlying mutation is known to succeed.

The implementation also checks `size_t` cardinality exhaustion only for a
genuinely new insertion, so a duplicate insertion preserves its no-op contract.

## Rank and select

Containers are globally ordered by their high 16-bit keys.

### rank(value)

Production sums cardinalities of all preceding containers, then counts lows at
most the target low inside the matching container.

Sparse payloads use `upper_bound`.

Dense payloads popcount preceding 64-bit words plus the masked prefix of the
target word.

### select(index)

Production walks containers in high-key order, subtracting complete container
cardinalities until the target container is reached.

Sparse payload selection is direct array indexing.

Dense payload selection walks bitmap words by popcount, then clears lower set
bits until the requested set bit becomes the least significant one.

Both operations return exact ordered-set semantics; they are not approximate
rank/select summaries.

## Structural replay

`valid_structure()` verifies:

- high keys are strictly increasing;
- no stored container is empty;
- global cardinality sum equals `size()`;
- sparse payload size equals container cardinality;
- sparse payload cardinality is at most 4096;
- sparse lows are sorted and unique;
- dense payload has exactly 1024 words;
- dense cardinality is greater than 4096;
- dense popcount equals stored container cardinality.

The replay is diagnostic evidence and is excluded from operation-complexity
claims.

## Independent verification

Committed tests compare the implementation against an independent
`std::set<uint32_t>` oracle.

Coverage includes:

- empty-set behavior;
- keys `0`, `65535`, `65536`, adjacent high partitions, and
  `UINT32_MAX`;
- duplicate insertion;
- absent erasure;
- exact rank and zero-based select;
- container ordering across multiple high prefixes;
- insertion of 4096 values in one container while remaining sparse;
- insertion of the 4097th value and conversion to dense bitmap;
- dense membership, rank, and select;
- erasure back to 4096 and conversion to sparse array;
- re-crossing the sparse/dense threshold;
- 18,000 fixed-seed randomized insert/erase operations against
  `std::set<uint32_t>`;
- periodic membership, rank, select, cardinality, and full structural replay.

Randomized values deliberately mix a few hot high-16 partitions with full-range
32-bit keys so both within-container and across-container behavior are exercised.

## Complexity boundary

Let `C <= 65536` be the number of occupied high-key containers and let `A`
be the cardinality of a sparse container.

Container lookup uses binary search over the sorted container vector:
`O(log C)` comparisons.

Because containers themselves live in a vector, inserting or removing an entire
high-key container may shift `O(C)` container records.

Within a sparse container:

- membership is `O(log A)`;
- insertion/erasure may shift `O(A)` lows;
- rank is `O(log A)`;
- select is `O(1)`.

Within a dense container:

- membership and ordinary bit insertion/erasure are `O(1)` after container
  lookup;
- rank scans at most 1024 machine words;
- select scans at most 1024 machine words;
- representation conversion scans one 65,536-value container domain.

Global rank and select additionally walk preceding containers, so this baseline
does not claim constant-time global rank/select.

Payload storage is:

- two bytes per stored low in sparse containers, excluding vector overhead;
- exactly 8192 bitmap payload bytes per dense container.

This slice makes no benchmark or cache-efficiency claim.

## Non-claims

This slice does not claim:

- official Roaring serialization compatibility;
- run containers;
- SIMD/vectorized operations;
- compressed on-disk format;
- copy-on-write persistence;
- thread safety;
- lock-free updates;
- set union/intersection acceleration;
- benchmark-backed throughput;
- succinct `n + o(n)` bit-space bounds.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/data_structures/roaring_style_bitmap.hpp`;
- `tests/test_roaring_style_bitmap_cases.hpp`;
- `docs/scope_recovery_roaring_style_bitmap.md`.

Pre-PR hardening includes:

- explicit `<utility>` inclusion for move operations;
- cardinality-capacity checking only for genuinely new values;
- transactional sparse/dense payload conversion so allocation failure does not
  first commit the logical update.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `cebf6850e0873e6d1aa5c0beb703265d5c9729da`.
