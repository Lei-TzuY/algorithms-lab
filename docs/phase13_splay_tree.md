# Phase 13 — splay-tree ordered set and amortized contracts

Phase 13 introduces a self-adjusting ordered data structure whose shape is part of the executable behavior. `SplaySet` stores unique signed 64-bit keys in a binary-search tree and restructures the tree after accesses instead of maintaining an explicit balance field.

## Public contract

`SplaySet` provides set-style `contains`, `insert`, and `erase` operations, plus `size`, `empty`, an ordered `inorder_keys` snapshot, `root_key` for replaying the self-adjusting behavior, and `valid_invariants` for structural verification.

- Keys span the full `int64_t` domain; no sentinel key is reserved.
- Duplicate insertion returns false and splays the existing node to the root.
- Successful lookup splays the matching node to the root.
- Unsuccessful lookup splays the last visited search node when the tree is non-empty.
- Successful erasure first splays the target to the root, detaches its left and right subtrees, splays the maximum node of the left subtree, and joins the right subtree there.
- Unsuccessful erasure inherits the miss-access behavior and therefore may still restructure the tree.
- Copy and move operations are deliberately disabled in this first slice so ownership semantics are not implied without implementation.

## Rotation and splay invariant

Every rotation preserves in-order key order and updates both child and parent links. Splaying repeatedly applies one of the textbook cases until the accessed node has no parent:

1. **zig** — one rotation when the parent is the root;
2. **zig-zig** — two rotations in the same direction for left-left or right-right ancestry;
3. **zig-zag** — two opposite rotations for left-right or right-left ancestry.

After a completed splay, the accessed node is the root and has a null parent.

`valid_invariants()` checks the externally relevant structural obligations:

- an empty tree has size zero;
- a non-empty root has no parent;
- every child points back to the node that owns that child link;
- keys satisfy strict BST lower/upper bounds, so duplicates cannot exist structurally;
- exactly `size()` reachable nodes are observed. A count guard fails closed if traversal would exceed the recorded size instead of allowing a malformed cycle/alias to run indefinitely.

## Erase join invariant

Once the erased root is removed, every key in the detached left tree is less than every key in the detached right tree. If the left tree exists, its maximum is splayed to become the new root. That root cannot have a right child inside the left tree, so attaching the original right tree preserves strict BST order. If no left tree exists, the original right tree becomes the root directly.

## Complexity boundary

A single search, insertion, or erasure can traverse and rotate through a linear-height tree, so this implementation makes the honest **worst-case `O(n)` time per individual operation** claim. It does not claim AVL/red-black-style worst-case logarithmic operations.

The standard splay-tree access lemma supplies the amortized argument: with uniform positive node weights, the potential based on subtree ranks bounds a splay by `O(log n)` amortized cost, and insertion/erasure add only constant structural work around accesses/splays. Therefore a sequence of ordinary set operations has `O(log n)` amortized cost per operation. This theorem is a mathematical proof obligation; the randomized tests below are not presented as empirical proof of the asymptotic bound.

`inorder_keys()` and `valid_invariants()` are explicit diagnostic/snapshot operations with `O(n)` time and `O(n)` auxiliary storage in the current iterative implementations. Destruction is also `O(n)`.

## Verification

Deterministic tests cover empty behavior, insertion and duplicate semantics, successful access-to-root behavior, unsuccessful last-visited splaying, zig-zig and zig-zag restructuring, erase split/join paths, sequential hot accesses, and the full `int64_t` key domain.

The differential test executes 20,000 fixed-seed mixed insert/erase/contains operations against an independent `std::set<int64_t>` oracle. After every operation it checks size and all structural invariants; ordered snapshots are compared periodically and at the end. Successful lookups additionally require the requested key to be the returned root.

Focused GCC and Clang builds use the repository strict warning policy. The same suite is also run under AddressSanitizer + UndefinedBehaviorSanitizer with leak detection enabled. Repository-wide integration remains the GitHub Actions gate before merge.

## Sealed boundary

Phase 13 is sealed after the splay-tree slice reached merged `main` with the full GCC/Clang/ASan+UBSan matrix green. The phase goal was not to enumerate amortized structures; it was to make mutation-driven self-adjustment and the distinction between individual worst-case cost and theorem-derived amortized cost explicit and executable. Adding another structure solely for name coverage would not establish a comparably new correctness boundary.

The next frontier therefore moves to persistence/versioning, where the central proof obligation changes from amortized restructuring to immutable historical versions, structural sharing, and transactional version creation.
