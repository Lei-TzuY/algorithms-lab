# Scope recovery — radix heap and monotone-integer Dijkstra

## Coverage decision

A fresh post-vertex-connectivity audit found no radix heap, monotone integer
priority queue, or radix-heap Dijkstra implementation in live code, pull-request
history, or recovery branches. Packed rank/select was rejected as already sealed;
persistent DSU was also available as a possible gap, but would extend an already
deep DSU family. This slice instead adds a distinct integer-key bucket invariant
and integrates it with shortest paths.

The occupied fractional-cascading, minimum-cycle-basis, general-graph-isomorphism,
and van-Emde-Boas recovery branches are untouched. The Phase-45--69
compiler/backend surface remains frozen under `scope_recovery_after_phase69.md`.

## Production contract

`RadixHeap<T>` is a first-principles monotone priority queue over `uint64_t` keys.
After the heap has popped key `last`, every subsequent push must have
`key >= last`; violating this contract throws `std::invalid_argument`.

The heap exposes push, pop, size/empty, last-popped key, redistribution count,
and an expensive `valid_structure()` diagnostic. It supports the full uint64
key domain. It intentionally has no decrease-key API: clients may insert a new
entry and discard stale copies when popped.

`radix_heap_dijkstra(graph, source)` applies that queue to the existing Graph:

- every edge weight is validated globally non-negative, including unreachable
  edges;
- self-loops and parallel edges retain ordinary Graph semantics;
- unreachable vertices remain `nullopt`;
- finite distances and parent witnesses use the existing signed-64 Weight domain;
- a required relaxation above `INT64_MAX` fails closed with `overflow_error`.

## Radix invariant / amortized obligation

Let `last` be the last key returned by pop. Bucket zero contains exactly keys
`== last`; for any larger key, bucket index is
`bit_width(key xor last)` in `1..64`.

When bucket zero is empty, pop chooses the lowest non-empty bucket, advances
`last` to that bucket's minimum key, and redistributes only that bucket under the
new xor prefix. Higher buckets keep their index because old and new `last` agree
above the redistributed bucket's highest differing bit.

A moved entry enters a strictly lower-numbered bucket. Therefore one entry is
redistributed at most 64 times over its lifetime. Push is O(1); pop is O(64)
amortized for fixed 64-bit keys (equivalently O(log C) redistributions over key
universe magnitude C). Resident storage is linear in queued entries.

For Dijkstra, every successful relaxation from a settled/non-stale distance `d`
has candidate `>= d`, exactly satisfying the monotone-push precondition. With
lazy stale entries, at most one queue insertion occurs per successful relaxation,
so the direct bound is O(64*(V+E)) queue work plus graph traversal and O(E) worst-
case queued stale-entry storage.

## Verification

Focused final candidate passes GCC C++20 strict warnings-as-errors, Clang C++20
strict warnings-as-errors, and actual GCC ASan+UBSan.

Evidence includes empty-pop and monotone-push rejection, duplicate keys, full
`UINT64_MAX`, redistribution diagnostics, and a 30,000-operation fixed-seed heap
trace against an independent `std::multiset` oracle while replaying the bucket
invariant after every operation.

Shortest-path evidence covers zero weights, self-loops, parallel edges,
unreachable vertices, globally unreachable negative-edge rejection, and signed
64-bit distance overflow. The primary randomized oracle is an independent
Bellman-Ford-style repeated relaxation on 700 fixed-seed directed/undirected
multigraphs with 1..25 vertices. Distances are also cross-checked against the
sealed BinaryHeap-based Dijkstra, while every returned radix parent edge is
replayed against the original graph.

## Non-claims

This is not a general unordered priority queue: inserting a key below the last
popped key is invalid. No floating-point/signed-key radix ordering, decrease-key,
thread safety, strong allocation-failure transaction guarantee, or benchmarked
speedup over BinaryHeap is claimed. The Dijkstra integration does not replace the
sealed generic implementation; it exposes the monotone-integer tradeoff for
comparison.
