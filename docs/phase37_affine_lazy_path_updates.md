# Phase 37 — affine lazy represented-path updates

Phase 37 extends the sealed Phase-36 link-cut forest with uniform represented-path addition while preserving the existing topology, point assignment, path assignment, distance, and exact path-sum contracts.

## Public contract

`add_path_value(u, v, delta)` exposes the unique represented path and adds the signed `int64_t` `delta` to every node on that path. Disconnected endpoints are rejected. Numeric state is transactional: if any affected node would leave the `int64_t` domain, the operation throws before any represented node value changes. As with every link-cut query, preferred-path exposure may still rearrange auxiliary splay structure.

The preflight is based on the exposed auxiliary subtree's exact logical minimum and maximum node values, not on the path aggregate. A path sum may remain representable while one individual value would overflow; that update must still be rejected.

## Lazy-tag composition

Each auxiliary node carries three independent logical transforms/state components:

- lazy reversal, which changes order but not values;
- optional uniform assignment, which replaces every logical value in the auxiliary subtree;
- a signed two-limb pending addition, which shifts every logical value in the auxiliary subtree.

Assignment dominates prior addition and therefore clears the pending-add tag. Addition after assignment is folded into the assignment value. Addition without assignment accumulates in the same exact signed two-limb representation used for wide aggregate arithmetic, so the deferred delta itself is not artificially restricted to `int64_t` after multiple valid updates. Reversal commutes with both numeric transforms.

When a lazy addition is applied to an auxiliary subtree, the stored exact sum changes by `delta * auxiliary_size`, while stored logical minimum and maximum are shifted by the same delta. Propagation applies assignment before addition and then reversal.

## Proof obligations / invariants

1. **Represented topology:** link/cut and connectivity semantics remain unchanged from Phase 34.
2. **Exact aggregate:** `auxiliary_sum` is the exact signed sum of all logical values in the auxiliary subtree; public narrowing happens only in `path_sum`.
3. **Value range:** `auxiliary_min` and `auxiliary_max` are the exact logical minimum/maximum values of that auxiliary subtree.
4. **Transactional addition:** if `min + delta` and `max + delta` are both representable, every intermediate logical value is representable; otherwise no represented value is modified.
5. **Assignment/addition order:** assignment overwrites older numeric tags; later addition transforms the assignment result. Multiple deferred additions compose additively in wide state.
6. **Lazy reversal:** reversing an auxiliary path changes only order and the reversal bit; sum/min/max and numeric tags are invariant under reversal.
7. **Auxiliary integrity:** parent/child links, auxiliary sizes, exact sums, value ranges, and inherited lazy-state semantics remain internally consistent under rotations/access.

## Complexity boundary

The link-cut forest retains the standard amortized `O(log V)` bound per operation under the usual splay-tree access analysis. This is not a worst-case `O(log V)` per individual operation claim. The additional min/max and lazy-add fields are constant-size per node and are maintained in constant work per touched auxiliary node/rotation.

## Verification

Focused verification covers:

- deterministic assignment → addition → assignment ordering in both path directions;
- cut/relink after pending numeric updates;
- per-node `INT64_MIN` / `INT64_MAX` transactional rejection with post-failure value checks;
- wide deferred-add composition whose cumulative delta itself exceeds `int64_t` even though each logical node value stays representable;
- exact path-sum cancellation across extreme signed values;
- disconnected and out-of-range validation;
- a fixed-seed 30,000-operation mixed topology / point assignment / path assignment / path addition / connectivity / distance / path-sum differential trace against an independent eager adjacency-matrix + BFS oracle;
- auxiliary invariant validation after every randomized operation.

Remote full CI is the backward-compatibility gate for the sealed Phase-34/35/36 link-cut tests in addition to the new Phase-37 suite.

## Sealed boundary and next frontier

Phase 37 is sealed after implementation PR #91 passed GCC release, Clang release, and GCC ASan+UBSan on exact head `a5d843a59aa9285ca95406a6894fccc91da52d4b`, then squash-merged as `main@0d7d24702bc6d20ad62e3ed861cf29074b832380` with the merged-main CI matrix also fully green. The post-merge architecture audit found no unresolved lazy-tag, representability, topology, oracle-independence, sanitizer, or complexity-claim blocker.

The internal min/max fields are proof machinery for transactional addition and are deliberately not promoted as trivial path-min/path-max API variants. Phase 38 changes the correctness dimension instead: ordered represented-path navigation. The target is zero-based path rank selection whose answer depends on the auxiliary in-order sequence after `make_root(first)` and `access(second)`, with lazy state pushed while descending and the selected vertex splayed so the standard amortized link-cut bound remains the appropriate sequence-level claim.
