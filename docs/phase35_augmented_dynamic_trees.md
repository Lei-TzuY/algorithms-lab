# Phase 35 — augmented dynamic trees

Phase 35 extends the sealed Phase-34 first-principles link-cut forest with exact node-value aggregates while preserving the represented-tree versus auxiliary-splay separation.

## Executable capability

`LinkCutForest` keeps the existing topology surface (`link`, exact-edge `cut`, connectivity, and path edge distance) and adds:

- `assign_value(vertex, value)` for signed `int64_t` point assignment;
- `path_sum(first, second)` for the exact sum of node values on the unique represented path;
- aggregate verification inside `valid_auxiliary_invariants()`.

Queries continue to mutate preferred-path / auxiliary-splay structure without changing represented topology.

## Exact aggregate representation

Using `int64_t` for internal auxiliary sums would be incorrect: a preferred-path decomposition may temporarily group values whose partial sum is outside `int64_t` even when the final represented-path sum is representable. A deterministic example is `INT64_MAX + 1 - 1`.

The implementation therefore maintains each auxiliary sum as a private two-limb signed-magnitude value:

- two `uint64_t` limbs hold a 128-bit magnitude;
- one sign bit distinguishes positive/negative values;
- addition/subtraction is implemented from first principles without `__int128`, Boost, wraparound arithmetic, or floating point.

For any resident forest, an auxiliary aggregate contains at most `SIZE_MAX` nodes, each with magnitude at most `2^63`. On platforms where `size_t` is at most 64 bits (enforced by `static_assert`), the exact magnitude is strictly below `2^127`, so this representation is sufficient for every structurally possible aggregate.

Only the public `path_sum()` conversion narrows back to `int64_t`. It throws `std::overflow_error` exactly when the final mathematical path sum is outside `[INT64_MIN, INT64_MAX]`; internal intermediate magnitude alone is never a reason to reject a valid assignment or query.

## Aggregate invariant

For every auxiliary node `x`:

`sum[x] = sum[left[x]] + value[x] + sum[right[x]]`.

The value is independent of in-order reversal, so the existing lazy path-reversal operation swaps children but does not alter the aggregate. Every rotation calls `pull` on the demoted parent and promoted child. `access` pulls after replacing a preferred right child, and point assignment exposes/splays the vertex before replacing its own value and pulling once.

`valid_auxiliary_invariants()` independently recomputes both auxiliary subtree size and exact aggregate in post-order in addition to the sealed Phase-34 child/parent and acyclicity checks.

## Path-sum query

For connected vertices `u` and `v`:

1. `make_root(u)` exposes `u` as represented root;
2. connectivity is validated through the existing root-finding contract;
3. `access(v)` exposes exactly the represented `u..v` path as `v`'s auxiliary tree;
4. the exact auxiliary aggregate at `v` is narrowed only at the public boundary.

Disconnected queries are rejected with `std::invalid_argument` exactly like the existing path-distance surface.

## Verification

Focused pre-upload verification uses the same strict warning policy as the repository:

- GCC 14 C++20: 4/4 focused groups pass;
- Clang 17 C++20: 4/4 pass;
- GCC ASan+UBSan with leak detection: 4/4 pass.

Deterministic regressions cover:

- point assignment and bidirectional path sums through topology changes;
- disconnected/bounds validation;
- exact cancellation across `INT64_MAX + 1 - 1`;
- positive and negative out-of-range path results;
- exact `INT64_MIN` singleton values;
- aggregate invariants after exceptions and repeated evert-style exposure.

Randomized differential verification runs 35,000 mixed operations over 40 vertices. An independent adjacency-matrix forest plus BFS reconstructs paths and sums bounded random values. The trace mixes link, cut, point assignment, connectivity, path distance, and path sum; the link-cut auxiliary invariant diagnostic is checked after every operation.

## Complexity boundary

Point assignment and path-sum queries use the same link-cut access/splay machinery as the sealed topology operations. Standard link-cut-tree analysis therefore gives amortized `O(log V)` per operation over a sequence and `O(V)` resident node storage. One individual operation may still take `O(V)` before amortization; no worst-case `O(log V)` claim is made.

The private exact aggregate expands each node by constant-size two-limb state, so it does not change the asymptotic storage bound.

## Claim boundary and seal

Phase 35 is **SEALED** after implementation candidate `dc5e4595337395c032c42c7a252c0e28cd09d52a` passed full-repository CI run `34325300351`, merged as `main@aa91765285541c8e209d171d9f027b8c5941c7e7`, and merged-main CI run `34325821036` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

The sealed capability is exact node-value point assignment plus represented-path sum over the existing dynamic-forest topology. The audit found no correctness, representability, sanitizer, or complexity-claim blocker. Path min/max variants are intentionally not added merely to enlarge the API.

Phase 36 promotes a genuinely new mechanism: **lazy represented-path assignment**. A uniform path assignment must compose correctly with access/splay and lazy reversal, update the exact auxiliary sum as path length times the assigned value without narrowing intermediate state, and preserve the existing amortized link-cut complexity contract.
