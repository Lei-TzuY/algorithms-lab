# Scope recovery: replayable order-statistic treap

## Recovery decision

The post-Phase-69 recovery audit found that the repository already contains
persistent segment trees, byte wavelet matrices, B-trees, van Emde Boas sets,
splay trees, link-cut trees, Fibonacci heaps, and other structural families, but
no treap / randomized BST capability. This slice fills that gap without resuming
the frozen compiler/backend roadmap.

The implementation is a first-principles set over signed 64-bit keys. It combines
BST order with a replayable pseudo-random heap priority and maintains subtree
sizes so rank and k-th-order-statistic queries are part of the same invariant,
not a follow-up wrapper.

## Executable contract

- `insert`, `erase`, and `contains` implement set semantics; duplicate insertion
  is a no-op and does not consume the priority stream.
- `rank(key)` returns the number of stored keys strictly smaller than `key`.
- `kth(i)` returns the zero-based i-th smallest key, or no value out of range.
- the same seed plus the same sequence of successful insertions produces the
  same priority stream and therefore the same structural preorder.
- split partitions a tree into `< key` and `>= key`; merge is used only when every
  key in the left tree is smaller than every key in the right tree.
- equal 64-bit priorities are resolved deterministically by key, making the heap
  order total without changing set semantics.

## Invariants and proof obligations

Every node simultaneously satisfies three invariants:

1. **BST order:** all left-subtree keys are smaller and all right-subtree keys are
   larger.
2. **Priority heap:** the `(priority, key)` order of a parent is strictly greater
   than that of either child.
3. **Subtree cardinality:** `size(node) = 1 + size(left) + size(right)`.

`split` descends one search path, reconnects only the edge on that path, and
refreshes subtree sizes while unwinding. `merge` chooses the higher-priority root
and recursively merges only the boundary subtrees, preserving both key order and
heap order. Insertion is split + singleton + merge; deletion replaces the found
node with the merge of its left and right subtrees. Rank and k-th queries follow
subtree sizes and do not mutate the tree.

## Complexity boundary

Operations take `O(h)` time where `h` is the current treap height; diagnostics
that materialize all keys take `O(n)`. The deterministic SplitMix64 stream makes
a run replayable. Under the standard random-priority treap model, expected height
and therefore expected search/update/order-statistic time are `O(log n)`, but the
implementation does **not** claim a deterministic worst-case logarithmic bound or
cryptographic randomness. Worst-case height, recursion depth, and operation time
remain `O(n)`.

## Verification

- empty/singleton and full signed-key boundary cases;
- duplicate insertion, missing deletion, rank and k-th boundaries;
- same-seed structural replay through preorder diagnostics;
- 20,000 fixed-seed mixed operations differentially checked against `std::set`
  and sorted-order statistics;
- 5,000 increasing insertions followed by repeated deletion of the current root,
  with structural invariants replayed throughout;
- strict GCC and Clang warning gates plus ASan/UBSan focused execution before the
  remote full-repository CI gate.

The standard library is used only as an independent testing oracle; it is not the
implementation under study.
