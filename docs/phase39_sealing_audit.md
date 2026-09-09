# Phase 39 sealing audit — rooted virtual-subtree accounting

## Gate evidence

Phase 39 is sealed only after the rooted-subtree cardinality implementation reached merged `main` and the exact merged-main CI matrix completed successfully.

- implementation PR #95 exact head: `f73df195b4b6645839b2c6d800eacd8d687b4223`;
- PR CI run `34340962400`: GCC release, Clang release, and GCC ASan+UBSan all completed successfully;
- squash-merged main commit: `6f84780946363a909412e26e03465a96328e60a7`;
- merged-main CI run `34341428259`: GCC release, Clang release, and GCC ASan+UBSan all completed successfully.

No red or pending implementation gate is being waived by this audit.

## Capability audit

Phase 39 adds a new represented-tree invariant rather than another path-query wrapper. The sealed link-cut forest now distinguishes:

- `auxiliary_size`, which counts only the current auxiliary splay subtree and remains the Phase-38 path-order index;
- `virtual_size`, which accounts for represented children outside the current preferred path;
- `represented_size`, which combines the node, both auxiliary children, and all virtual-child contributions.

`access` transfers the old preferred right child into virtual accounting and removes the newly preferred child from it. `link` adds the attached represented component as a virtual contribution, while an exposed-edge `cut` removes the detached preferred contribution through the normal pull path. Rotations and lazy reversal preserve represented topology, so cardinality is recomputed structurally rather than duplicated in a second explicit forest.

The public `rooted_subtree_vertex_count(root, vertex)` query reroots at `root`, exposes `vertex`, and subtracts the represented contribution of the exposed ancestor side. That leaves exactly `vertex` plus its rooted represented descendants.

## Integration audit

The phase remains integrated with every sealed dynamic-tree capability from Phases 34–38:

- link/cut and connectivity;
- represented path edge distance;
- point value assignment;
- lazy path assignment and addition;
- exact path sums and public overflow behavior;
- zero-based represented-path navigation;
- lazy reversal and auxiliary structural diagnostics.

Cardinality is intentionally orthogonal to numeric lazy state. Path assignment/addition changes logical node values but cannot change represented-tree vertex counts, and the mixed verification trace exercises both kinds of state together.

## Verification independence

The dedicated tests do not use another link-cut implementation as an oracle. They maintain an eager adjacency-matrix forest, root it with BFS, and count descendants directly.

Deterministic cases cover chains, stars, root changes, disconnection, cut/relink, and pending lazy numeric tags. A 3,000-step fixed-seed mixed trace interleaves topology changes, point/path updates, path sums, Phase-38 kth navigation, and rooted-subtree queries while repeatedly checking auxiliary/represented cardinality invariants.

## Complexity and claim boundary

Only constant-time represented-cardinality bookkeeping is added to access/rotation/link/cut machinery, so the sealed claim remains the standard link-cut **sequence-level amortized `O(log V)`** operation bound with `O(V)` resident storage. One individual operation may still take linear time in auxiliary height, and `push_path` may use temporary space proportional to that height. CI timing is not used as asymptotic evidence.

## Why the phase stops here

Additional cardinality aliases such as component-size wrappers would reuse the same invariant without adding architectural depth. Phase 39 therefore stops once virtual-child cardinality is independently verified and integrated.

The next substantial gap is **rooted represented-subtree numeric aggregation**. The forest already supports exact node values and lazy represented-path assignment/addition, but Phase 39 tracks only virtual cardinality. An exact rooted-subtree value sum requires virtual children to carry exact numeric contributions too, while path-lazy updates must change only values on the exposed auxiliary path and leave virtual descendants untouched. That interaction is a genuinely new aggregate invariant and justifies Phase 40 as a separate frontier.
