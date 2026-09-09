# Phase 38 sealing audit — ordered represented-path navigation

## Gate evidence

Phase 38 is sealed against merged `main` commit `a1cf0bdec718201000d0784f95f1393937ed791c`.

- implementation PR #93 exact head: `3b99695647bd962b035f03fa1ec01c595db6e1f2`
- PR CI run `34337389633`: GCC release, Clang release, GCC ASan+UBSan all completed successfully
- squash-merged main commit: `a1cf0bdec718201000d0784f95f1393937ed791c`
- merged-main CI run `34337793152`: GCC release, Clang release, GCC ASan+UBSan all completed successfully

No red or pending implementation gate is being waived by this audit.

## Capability audit

The phase adds one coherent capability: zero-based navigation by represented-path order.

The implementation reuses the already-sealed link-cut `auxiliary_size` invariant. It does not maintain a duplicate order index. After rerooting at `first` and exposing `second`, the represented path is the auxiliary splay in-order sequence from `first` to `second`; rank descent pushes deferred lazy state before choosing left/current/right by stored subtree size, then splays the selected vertex.

This remains integrated with all previously sealed dynamic-tree behavior:

- `link` / `cut` and connectivity;
- path edge distance;
- point assignment;
- lazy represented-path assignment and addition;
- exact path sums and public overflow behavior;
- lazy reversal;
- auxiliary structural/aggregate invariant diagnostics.

Navigation is intentionally structurally mutating, just like the other link-cut queries, but it does not change represented topology or logical node values.

## Verification independence

The long mixed trace does not use another link-cut implementation as an oracle. It stores represented edges in an eager adjacency matrix, reconstructs unique paths with BFS, keeps eager point values, and checks:

- every selected rank against BFS path order;
- path sums against eager path values;
- disconnected and out-of-range rejection;
- repeated auxiliary invariants while topology and lazy numeric state are changing.

Deterministic regressions separately cover reverse order, singleton paths, exact boundary ranks, pending assignment/addition state, and cut/relink topology changes.

## Complexity and claim audit

The sealed claim remains the standard link-cut **sequence-level amortized `O(log V)`** operation bound. The phase does not claim worst-case `O(log V)` for one splay, and the existing `push_path` helper may use temporary space proportional to auxiliary height.

No benchmark timing is used as asymptotic evidence, and no path-navigation result is presented as a rooted-subtree result.

## Why the phase stops here

Adding wrappers for already-maintained path min/max state, aliases for first/last path vertices, or multiple near-identical rank APIs would increase surface area without a new invariant. Phase 38 is therefore sealed after ordered selection rather than farming path-query variants.

The next substantial dynamic-tree gap is **virtual-child accounting**. Rooted subtree cardinality cannot be derived from the exposed preferred path alone: `access` must preserve represented contributions of children that move between preferred and virtual status, and `link` / `cut` must maintain the same invariant. That new bookkeeping and its independent rooted-BFS oracle justify Phase 39 as a separate architectural frontier.
