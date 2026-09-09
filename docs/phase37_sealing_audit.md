# Phase 37 sealing audit

## Integrated evidence

- Phase-37 implementation PR #91 exact head: `a5d843a59aa9285ca95406a6894fccc91da52d4b`.
- Exact PR CI run `34333446221` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.
- The implementation was squash-merged as `main@0d7d24702bc6d20ad62e3ed861cf29074b832380`.
- Exact merged-main CI run `34334043736` completed successfully on the same three jobs.
- Verification includes deterministic assignment/addition ordering, cut/relink behavior with deferred numeric state, per-node `INT64_MIN`/`INT64_MAX` transactional rejection, wide cumulative pending-addition composition, exact cancellation, and a fixed-seed 30,000-operation mixed trace against an independent eager adjacency-matrix + BFS forest oracle.
- `valid_auxiliary_invariants()` is checked throughout the randomized trace.

## Correctness and architecture audit

The implementation closes the Phase-37 representability gap at the correct level. Uniform path addition is preflighted against the exposed path's exact logical minimum and maximum node values. Checking only the path sum would be insufficient: a final aggregate may be representable while one individual node would overflow. For a uniform shift, representability of both `min + delta` and `max + delta` is necessary and sufficient for every affected logical value to remain in `int64_t`.

The lazy-state composition is coherent:

- assignment overwrites prior pending numeric addition;
- addition after assignment is folded into the assignment result;
- addition without assignment accumulates in the existing exact signed two-limb representation;
- reversal changes path order but commutes with the numeric transforms;
- propagation applies assignment, then addition, then reversal.

The exact auxiliary sum and logical min/max state are maintained through access, splay rotations, and lazy propagation. The invariant diagnostic independently reasons through inherited assignment/addition state rather than relying on mutation to make stale descendants current.

A failed numeric update is transactional with respect to represented node values. Ordinary path exposure may still rearrange the auxiliary preferred-path representation before a validation failure; the API does not falsely claim structural transactionality of the splay representation.

The source-level audit of `apply_assignment`, `apply_addition`, `pull`, `push`, path exposure, preflight validation, and invariant checking found no unresolved correctness, representability, topology, or sanitizer blocker.

## Claim boundary

The sealed complexity contract remains the standard link-cut-tree sequence-level amortized `O(log V)` bound per operation with `O(V)` resident storage. One individual operation may still take `O(V)` before amortization. CI timing is not used as complexity evidence.

`auxiliary_min` and `auxiliary_max` are retained as proof machinery for safe addition; exposing path-min/path-max queries merely because those fields now exist would be low-value surface farming and is deliberately excluded from Phase 37.

Phase 37 therefore ends here rather than adding another naming-adjacent numeric update or aggregate variant.

## Promotion

Phase 38 moves to a different correctness dimension: **ordered represented-path navigation**.

The first executable target is zero-based `kth_vertex_on_path(first, second, rank)`. After `make_root(first)` and `access(second)`, the exposed auxiliary splay's in-order sequence must represent the unique path from `first` to `second`. Rank selection must push deferred reversal/assignment/addition state while descending by stored auxiliary subtree sizes, reject disconnected endpoints and out-of-range ranks, preserve represented topology and numeric semantics, and splay the selected vertex so the standard amortized link-cut bound remains the appropriate claim.

Verification must cover forward and reversed path order, singleton/boundary ranks, pending lazy numeric state, topology changes, and long fixed-seed mixed traces against an independent BFS path-vector oracle.
