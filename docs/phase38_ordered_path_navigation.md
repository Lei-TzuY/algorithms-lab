# Phase 38 — ordered represented-path navigation

## Executable contract

`LinkCutForest::kth_vertex_on_path(first, second, rank)` returns the zero-based `rank`-th represented-tree vertex on the unique path from `first` to `second`.

- `rank == 0` returns `first`.
- `rank == path_edge_distance(first, second)` returns `second`.
- A disconnected pair throws `std::invalid_argument`.
- A rank outside the exposed path throws `std::out_of_range`.
- The query may rearrange the auxiliary splay forest, as existing link-cut queries do, but it does not change represented topology or stored numeric values.

## Path-order invariant

After `make_root(first)` and a successful connectivity check, `access(second)` exposes exactly the represented path from `first` to `second`. The in-order sequence of the resulting auxiliary splay rooted at `second` is that represented-path order.

The sealed link-cut structure already maintains `auxiliary_size = 1 + left_size + right_size` through every pull/rotation/access step. Rank selection therefore does not add a second indexing structure:

1. Push the current node before inspecting children so deferred reversal, assignment, and addition state is materialized in the direction relevant to navigation.
2. Let `L` be the stored size of the current left auxiliary subtree.
3. Descend left when `rank < L`; return the current vertex when `rank == L`; otherwise set `rank -= L + 1` and descend right.
4. Splay the selected vertex before return, preserving the standard self-adjusting link-cut access discipline.

Because reversal swaps auxiliary left/right children, pushing deferred reversal state while descending is part of the correctness obligation; reading subtree sizes without push-down would not be enough to preserve first-to-second ordering.

## Interaction with lazy numeric state

Path navigation is value-neutral. It nevertheless calls the existing `push()` machinery during descent, so pending path assignment/addition tags may move from an ancestor to children. That is only a representation change: exact values, aggregate sums, min/max evidence, and represented topology must remain unchanged.

Tests therefore combine navigation with pending assignment/addition, reverse-direction queries, path-sum checks, cut/relink operations, and `valid_auxiliary_invariants()`.

## Verification

The dedicated regression suite covers:

- forward and reverse order on deterministic paths;
- singleton paths and boundary ranks;
- disconnected and out-of-range rejection;
- pending path assignment plus addition before ordered navigation;
- topology changes via cut and relink;
- a long fixed-seed mixed trace with link/cut, point assignment, path assignment, path addition, path sum, and ordered navigation.

The long trace maintains an independent eager adjacency matrix and reconstructs every oracle path with BFS. Rank answers and path sums are checked against that oracle rather than another link-cut representation.

## Complexity boundary

Once the represented path is exposed, rank descent follows one auxiliary-tree root-to-node path using stored subtree sizes and then splays the selected node. Together with `make_root/access/splay`, this retains the standard link-cut **amortized `O(log V)`** sequence-level operation bound.

No worst-case `O(log V)` per-operation claim is made: an individual auxiliary splay can still be linear in its current height. The existing `push_path` implementation also uses temporary storage proportional to that auxiliary height while propagating lazy state.

## Sealed boundary

Phase 38 is sealed only after the implementation reached merged `main` at `a1cf0bdec718201000d0784f95f1393937ed791c` and push CI run `34337793152` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. The phase establishes ordered represented-path selection; it does **not** claim rooted-subtree accounting, path-extremum selection, or worst-case logarithmic splay operations.

The next frontier deliberately changes the maintained invariant rather than adding another path accessor: Phase 39 introduces virtual-child represented-size accounting so rooted subtree cardinality can be answered while preferred paths continue to change.
