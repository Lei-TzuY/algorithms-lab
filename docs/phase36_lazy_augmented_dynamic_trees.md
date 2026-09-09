# Phase 36 — lazy augmented dynamic trees

Phase 36 extends the sealed Phase-35 augmented `LinkCutForest` with a genuinely new mutation mechanism: a uniform assignment over an entire represented path. The slice preserves dynamic topology, point assignment, exact path sums, and the standard link-cut amortized boundary while adding a deferred numeric tag that must coexist with auxiliary splay restructuring and lazy path reversal.

## Executable capability

`LinkCutForest` adds:

- `assign_path_value(first, second, value)` to replace every node value on the unique represented path with one signed `int64_t` value;
- exact `path_sum` after arbitrary interleavings of path assignment, point assignment, `link`, `cut`, connectivity, and path-distance queries;
- lazy-aware auxiliary invariant verification without forcing the diagnostic to mutate or materialize pending tags.

A path assignment requires connected endpoints and rejects disconnected or out-of-range vertices using the existing validation conventions.

## Path exposure contract

Every assignment, including a singleton path, uses the normal represented-path exposure sequence:

1. `make_root(first)`;
2. verify `find_root(second) == first`;
3. `access(second)`;
4. apply the assignment to the exposed auxiliary tree rooted at `second`.

This uniform rule is correctness-critical. A prototype singleton shortcut that used only `access(first)` was rejected after a deterministic regression showed it could assign other vertices already present in the same preferred-path auxiliary tree. The production candidate keeps that regression and has no singleton shortcut.

## Lazy assignment invariant

Each auxiliary node carries an optional pending assignment value. If a node has a pending assignment `a`, its entire current auxiliary subtree is logically uniform with value `a`, and its aggregate is exactly:

`sum[x] = a * auxiliary_size[x]`.

`apply_assignment` updates the node's visible value, stores the tag, and recomputes the aggregate immediately while leaving descendants deferred. `push` propagates the numeric assignment to both auxiliary children before clearing the parent tag. Point assignment first exposes the vertex so ancestor tags are materialized, then replaces the single value and clears any assignment tag on that splayed node.

Uniform assignment commutes with path reversal: reversal changes auxiliary in-order direction but not the set of nodes or their equal assigned value. The existing reversal tag can therefore coexist without changing the numeric tag's semantic value.

## Exact aggregate scaling

The sealed Phase-35 two-limb signed-magnitude aggregate remains the arithmetic domain. Path assignment computes `value * auxiliary_size` from first principles by binary doubling and exact magnitude addition; it does not use `__int128`, floating point, signed overflow, or intermediate `int64_t` narrowing.

The Phase-35 structural bound still applies: at most `SIZE_MAX` resident nodes, each with magnitude at most `2^63`, gives a total magnitude strictly below `2^127` on repository-supported platforms where `size_t` has at most 64 value bits. A pending uniform assignment therefore has an exact internal aggregate for every structurally possible exposed path.

Only the public `path_sum()` conversion narrows to `int64_t` and throws when the final mathematical path sum is genuinely outside the public range.

## Lazy-aware diagnostic invariant

A const invariant checker cannot simply require every descendant's stored value/aggregate to be materialized when an ancestor has a pending assignment. Instead the diagnostic traverses the auxiliary forest with an inherited logical assignment state:

- child/parent consistency, acyclicity, coverage, and stored auxiliary sizes are always checked;
- under an inherited pending assignment, the logical descendant aggregate is recomputed from the inherited value and subtree size without requiring stale stored descendants to match;
- at a node that itself owns the pending assignment, its visible value and stored aggregate must match that assignment and exact scaled sum;
- outside any pending assignment, the sealed Phase-35 exact recursive aggregate equality is enforced.

This verifies lazy semantics without calling `push` or mutating the forest merely to inspect it.

## Verification

Focused pre-upload verification uses the repository strict warning policy:

- GCC C++20 strict warnings: 4/4 focused groups pass;
- Clang C++20 strict warnings: 4/4 pass;
- GCC ASan+UBSan with actual sanitizer instrumentation: 4/4 pass.

Deterministic coverage includes:

- deferred path assignment followed by reversed-direction queries;
- overlapping path assignments and later point overwrite;
- topology changes while numeric tags have been exposed/pushed through link-cut operations;
- the singleton-path regression that caught the prototype auxiliary-subtree over-assignment bug;
- disconnected and bounds rejection without changing existing values;
- positive/negative public overflow and cancellation after path assignment;
- exact `INT64_MIN`/`INT64_MAX` boundaries through the existing wide aggregate domain.

The randomized differential runs 30,000 fixed-seed mixed operations over 40 vertices. An independent adjacency-matrix forest plus BFS reconstructs represented paths and performs eager naïve path assignment. The trace mixes `link`, `cut`, point assignment, path assignment, connectivity, path edge distance, and path sum; auxiliary invariants are checked after every operation.

## Complexity boundary

Path assignment is implemented by one represented-path exposure followed by an `O(1)` lazy tag application to the exposed auxiliary root. It therefore inherits the standard link-cut amortized `O(log V)` operation bound over a sequence and `O(V)` resident storage. A single operation may still take `O(V)` before amortization; no worst-case `O(log V)` claim is made.

The assignment tag and exact scaled aggregate add only constant-size state per node. No benchmark timing is used as asymptotic evidence.

## Phase boundary

Phase 36 is sealed after candidate `11884b4d073bfe11fbb9834bf99d1f6d1cc2daca` passed full-repository CI run `34328960851`, was squash-merged as `7238fb95a16bdf4cb9b60bbcc2277691265c6e77`, and the exact merged-main CI run `34329511772` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

Path addition, min/max, and generic affine lazy tags remain outside the sealed Phase-36 scope. They require different composition and per-node representability obligations and are not treated as free variants of uniform assignment.

The next frontier is Phase 37: affine lazy represented-path updates. Its first coherent slice adds uniform path addition while composing correctly with sealed assignment/reversal tags and transactionally rejecting any update that would move an individual node value outside `int64_t`.
