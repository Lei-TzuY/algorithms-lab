# Scope recovery: replayable cuckoo-hash ordered-key set

## Coverage decision

A fresh live audit at `main@df5bcda97cedcafe6f331e237067ec8894c6196a`
found no cuckoo / robin-hood / hopscotch hash-table implementation in default-
branch code, pull-request history, or the branch namespace. The audit first
considered a rebuild-based BST, but the sealed Phase-13 splay-tree boundary
explicitly rejects adding another amortized BST merely for name coverage. Two
unmerged Jacobi/eigensolver branches were also detected and left untouched.

This slice therefore changes structural proof model after the recent
Gallai-Edmonds / Tutte-Berge matching sequence: two legal hash locations,
displacement paths, exact rollback, and transactional rehashing. Prospective
authority remains `docs/scope_recovery_after_phase69.md`; the frozen
compiler/backend excursion and stale historical ROADMAP do not drive this work.

## Production contract

`algorithms::data_structures::CuckooHashSet` stores unique signed-64 keys.
Construction takes an explicit `uint64_t` seed and an initial per-table capacity;
capacity is normalized to a power of two with a minimum of four.

The public contract includes:

- exact `insert`, `erase`, and `contains` set semantics;
- two internal tables, with every resident key stored in exactly one of its two
  legal buckets;
- deterministic `sorted_values()` replay;
- deterministic `layout()` replay exposing `(key, table, slot)` diagnostics;
- per-table capacity, rebuild count, and rehash-generation diagnostics;
- `valid_structure()` replay of legal-bucket placement, cardinality, and
  duplicate absence.

Deletions do not shrink the table in this baseline. Copy/move/iterator,
concurrency, multiset multiplicity, and heterogeneous-lookup contracts are not
claimed.

## Cuckoo invariant and rebuild boundary

For current salts `s0` and `s1`, every key `k` must occupy exactly one of
`table0[h(k,s0)]` or `table1[h(k,s1)]`. Lookup and erase therefore inspect at
most those two slots.

Insertion first tries ordinary alternating cuckoo displacement. Before every
slot swap the previous slot value is recorded in a bounded write log. If an
empty slot is found, the insertion commits. If the kick budget is exhausted,
the log is replayed in reverse and the original table is restored exactly before
any rebuild attempt begins.

Rebuilding is transactional with respect to the existing set: production takes
a sorted snapshot of all resident keys plus the new key, allocates fresh
candidate tables, derives fresh salts from the caller seed and an explicit
rehash generation, and tries to place the complete key set there. Only a fully
successful candidate is moved into the live object. After every eight failed
salt attempts the per-table capacity doubles. A fixed 32-attempt budget bounds
this direct implementation; exhaustion throws `std::runtime_error` rather than
silently dropping or duplicating keys.

The mixer and salt schedule are repository-defined and deterministic, so the
same seed and operation trace replay the same successful layout. They are **not**
claimed to form a universal/cryptographic hash family. Consequently this slice
does not transfer the textbook expected-constant insertion theorem for ideal
independent hash functions onto this concrete mixer.

## Complexity and non-claims

- `contains` / `erase`: exactly two bucket computations/probes, `O(1)` fixed-word
  work;
- an insertion that does not rebuild performs at most `O(log C)` kicks for
  per-table capacity `C`;
- one rebuild attempt places `n` keys with at most `O(n log C)` bounded kicks;
  the implementation makes at most 32 attempts and at most four capacity
  doublings in one public rebuild call;
- resident storage is `O(C)` plus temporary `O(C+n)` during rebuild.

No expected-`O(1)` insertion, universal-hashing probability bound,
adversarial-resistance, cryptographic property, cache-performance result,
automatic shrinking, or benchmark speedup is claimed.

## Verification

Focused exact-candidate verification passed:

- GCC C++20 repository strict warnings-as-errors: 5/5;
- Clang C++20 repository strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with leak detection / halt-on-error: 5/5.

Deterministic evidence covers full signed-64 key boundaries, duplicate/absent
operations, growth rebuilds, exact same-seed layout replay, different-seed set
semantics, and a concrete displacement-cycle regression. For that regression,
`seed=16`, per-table capacity 64, and the sequence `k*1,000,003+17` triggers a
cycle/rebuild on the 31st key while capacity remains 64; every previous key and
the new key must still be present afterward. This directly exercises rollback
followed by temporary rehash rather than relying on load-factor growth.

The primary randomized oracle is `std::set<int64_t>`, which shares no cuckoo
placement/displacement logic. A fixed-seed trace performs 18,000 mixed
insert/erase/contains operations and periodically compares exact size, emptiness,
complete sorted contents, and all structural placement invariants. A separate
local stress corpus ran 20 independent seeds x 15,000 operations with the same
oracle/invariant checks.

Tests establish implementation evidence; they are not presented as a statistical
proof of hash-family performance.

## Scope

Exactly four paths are intended to differ from live main:

- `include/algorithms/data_structures/cuckoo_hash_set.hpp`;
- `tests/test_cuckoo_hash_set_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, occupied recovery surface, or temporary-file churn is
part of this slice.
