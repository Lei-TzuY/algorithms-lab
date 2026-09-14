# Scope recovery: exact alpha-beta minimax on finite game trees

## Coverage decision

A fresh post-Phase-69 coverage audit at exact green `main@7921e42cd89adfa3075173035d5ae3419726adb3` found no minimax or alpha-beta capability in live default-branch code, pull-request history, or branch names. The preceding dense QR least-squares checkpoint reached merged-main CI success in run `34807814204` on GCC release, Clang release, and GCC ASan+UBSan before this branch was created. The existing `games` surface contains parity-game and Sprague-Grundy solvers, whose infinite-play / impartial-DAG proof models differ from finite alternating zero-sum tree search. This slice therefore changes proof model after dense QR least squares rather than extending numerical linear algebra.

## Production contract

`alpha_beta_minimax(nodes, root, maximizing_root)` accepts one finite rooted ordered tree:

- every non-root node has exactly one parent and every supplied node is reachable from `root`;
- leaves carry one signed-64 utility and no children;
- internal nodes carry ordered children and no utility;
- the root may be a maximizing or minimizing turn and roles alternate by depth;
- equal-valued choices keep the first child in input order;
- the result returns the exact root minimax value, deterministic principal variation, visited-node count, evaluated-leaf count, and cutoff count.

Malformed child indices, duplicate-parent structure, cycles/disconnected components, and leaf/internal payload mismatches are rejected. Utilities are never added or multiplied, and optional alpha/beta bounds avoid artificial integer sentinels, so exact `INT64_MIN` and `INT64_MAX` utilities are supported.

## Correctness obligation

Without a cutoff, a maximizing node returns the maximum child value and a minimizing node the minimum child value. Alpha is a lower bound already available to maximizing ancestors; beta is the corresponding upper bound for minimizing ancestors. Once `alpha >= beta`, the remaining siblings cannot change any ancestor decision that reaches the current node, so pruning preserves the root minimax value. Strict-improvement updates preserve deterministic first-child tie semantics and the reconstructed principal variation.

The theorem is the proof obligation. Tests compare production with a full unpruned minimax evaluation; they are implementation evidence rather than a proof that pruning is sound.

## Verification

Focused repo-native candidate verification passed repository warning policy under GCC and Clang and actual ASan+UBSan, 4/4 in each configuration. Deterministic coverage includes:

- a hand-checkable tree where alpha-beta performs a real cutoff while matching full minimax;
- maximizing and minimizing roots at the signed-64 utility extremes;
- nonzero-root execution and deterministic first-child tie behavior;
- malformed empty/root/child-index/leaf/internal/disconnected/duplicate-parent shapes;
- principal-variation replay.

The primary randomized corpus contains 1,500 fixed-seed rooted trees with up to 80 nodes and random leaf utilities. An independent full minimax oracle evaluates every node without alpha/beta bounds. Production must match both exact value and first-child-tie principal variation, and may never visit/evaluate more nodes or leaves than the oracle.

## Complexity and non-claims

Tree validation is `O(V+E)`. Alpha-beta visits each reached tree node at most once, so worst-case search remains `O(V)` on this explicit tree representation; pruning only reduces the visited subset. Result/validation state is `O(V)` and recursive search uses `O(depth)` call stack.

This baseline does not claim a numeric best/average-case pruning exponent, transposition tables, iterative deepening, move ordering heuristics, chance nodes/expectiminimax, cyclic game graphs, simultaneous moves, parallel search, or stack-safe behavior on adversarially deep trees.
