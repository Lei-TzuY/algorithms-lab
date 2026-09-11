# Scope recovery: handle-based Fibonacci min-heap

## Coverage decision

A fresh live-code, roadmap-governance, and recent-commit audit after the exact
static 2D KD-tree checkpoint found no Fibonacci heap, meldable decrease-key heap,
or cascading-cut capability. The repository does contain the sealed Phase-1
array `BinaryHeap`, but that structure studies eager local heap-order repair and
provides no stable handles, decrease-key operation, lazy root consolidation, or
cut/cascading-cut state.

This recovery slice deliberately changes proof model again after the recent
KD-tree spatial-partition, B-tree occupancy-repair, and van-Emde-Boas universe-
decomposition checkpoints. The frozen Phase-45--69 compiler/backend surface
remains untouched. Historical active headings in `ROADMAP.md` are not prospective
authority; `docs/scope_recovery_after_phase69.md` plus a fresh coverage audit
remain the governing boundary.

## Production contract

`algorithms::data_structures::FibonacciMinHeap` stores signed 64-bit keys and
returns globally unique stable integer handles from insertion.

- `minimum()` returns the lexicographically deterministic `(key, handle)` minimum.
- `insert(key)` returns a handle that stays valid until that exact node is
  extracted or the owning heap is destroyed/overwritten.
- `decrease_key(handle, new_key)` rejects increases and performs the standard
  cut/cascading-cut repair when heap order is violated.
- `extract_min()` promotes the minimum root's children and consolidates equal-
  degree roots until at most one root of each degree remains.
- `meld(std::move(other))` destructively combines two heaps, empties `other`, and
  preserves handles originating in either heap for subsequent operations on the
  destination.
- `root_count()`, `marked_count()`, `potential()`, and `valid_structure()` expose
  executable state evidence rather than hiding the amortized proof state.

No standard-library priority queue or meldable-heap implementation is used for
production behavior. Ordered standard containers appear only as independent test
oracles.

## Structural invariants and proof obligations

Every active node belongs to exactly one circular doubly linked root or child
list. For every list entry, `left->right` and `right->left` return the node.
Every non-root node has the expected parent, and every root has a null parent and
is unmarked.

Heap order uses the same deterministic lexicographic `(key, handle)` ordering
that the public minimum exposes: no child may be ordered before its parent. This
matters when decrease-key makes a child key exactly equal to its parent key; an
earlier handle is then cut so the globally least pair remains a root. `degree`
equals the number of immediate children, and the public minimum pointer is the
root with the least `(key, handle)` pair.

A non-root node is marked only after it loses its first child while remaining a
child. Losing another child cuts that node and recursively applies the same rule
to its parent. Roots are always unmarked. The classical potential tracked for
diagnostics is

`Phi = number_of_roots + 2 * number_of_marked_nodes`.

`decrease_key` can therefore pay for cascading cuts by reducing accumulated
potential, while insertion adds one root. `extract_min` performs the deferred
work: after child promotion it repeatedly links roots of equal degree, making the
better `(key, handle)` root the parent.

These are the classical Fibonacci-heap preservation and amortized-analysis
obligations. Tests replay the concrete invariants and state transitions; they do
not substitute for the potential-method proof.

## Complexity and implementation boundary

For the linked heap structure itself, `minimum` is `O(1)`, insertion performs
constant structural work, decrease-key has the classical `O(1)` amortized cut
bound, and extract-min has the classical `O(log n)` amortized consolidation bound.
The implementation deliberately exposes an additional bookkeeping boundary:
stable integer handles are owned through a per-heap `unordered_map` so invalid or
expired handles can be rejected deterministically.

Because non-empty `meld` migrates `other`'s handle-ownership entries into the
destination map, **the public meld operation is `O(other.size())` expected time in
this educational baseline**, even though the circular root-list splice itself is
constant time. The destination reserves ownership capacity before any root-list
mutation, then transfers existing map nodes before the non-throwing structural
splice, so an allocation failure cannot occur after the two root lists have been
partially joined. No `O(1)` public meld claim is made. Handle lookup also inherits
`unordered_map` expected-time rather than worst-case-constant behavior.

Resident node state is `O(n)`. Consolidation uses an auxiliary degree table and a
root snapshot. The implementation makes no concurrency, lock-free, allocator,
cache-locality, worst-case decrease-key, worst-case constant-time meld, or
universal performance claim.

## Verification

Focused pre-upload verification passes under repository-equivalent settings:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- GCC ASan+UBSan with frame pointers enabled.

Deterministic coverage checks empty-heap exceptions, duplicate-key tie semantics,
an equal-key decrease that must cut on handle order, `INT64_MIN`, expired handles,
illegal key increases, move construction/assignment, consolidation, an observed
marked-node state, an observed multi-root cascading cut, and the exact potential
identity.

The primary randomized differential trace executes 5,000 fixed-seed mixed
insert/decrease/extract operations against an independent
`std::map<handle,key>` plus `std::multiset<(key,handle)>` oracle. Every operation
checks size, exact minimum semantics, and the full structural invariant replay.
A separate 120-trial randomized meld corpus verifies that handles created in the
source heap remain usable in the destination, then drains the merged heap against
the ordered oracle. The bounded corpus is intentional because
`valid_structure()` performs a full diagnostic traversal after every operation;
test-runtime cost is not presented as heap-operation benchmark evidence.
