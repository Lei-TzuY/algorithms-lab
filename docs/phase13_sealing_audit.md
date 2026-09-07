# Phase 13 sealing audit

Phase 13 is sealed after the splay-tree ordered-set slice completed its implementation, exact candidate CI, merge, and merged-main integration gates.

## Verified implementation checkpoint

- Implementation PR: #43, `Phase 13: splay-tree ordered set`.
- Exact candidate: `dd66966588c7f42a804d7e50cd96f4ba5913ed0c`, one commit ahead of the Phase-12 sealed base.
- Scope: six files only — public API, production source, tests, proof/contract documentation, CMake registration, and the Phase-13 ROADMAP checkbox.
- Candidate GitHub Actions run `34163856230` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.
- Normal merge produced `main@4d704f3a8ee9f5ef74965395e5559aa3452e26a9`.
- Merged-main GitHub Actions run `34164012365` completed successfully on the same three-job matrix.

## Capability boundary established

`SplaySet` adds a genuinely self-adjusting ordered-set surface rather than another static balanced tree. Successful accesses and insertions move the relevant node to the root; misses move the last visited search node; erasure performs an explicit split/splay/join sequence. Zig, zig-zig, and zig-zag rotations preserve strict BST order and reciprocal parent links.

The implementation also makes its complexity boundary explicit. A single operation can cost `O(n)` on a linear-height tree. The `O(log n)` statement is amortized over an operation sequence and rests on the standard splay access lemma; test timing is not used as proof of that theorem.

## Evidence boundary

Deterministic regressions cover hit/miss restructuring, duplicate insertion, erase joins, zig-zig/zig-zag cases, sequential hot accesses, and the full `int64_t` key domain. A 20,000-operation fixed-seed differential trace compares membership and order with `std::set<int64_t>` while checking size and structural invariants after every operation. Focused GCC, Clang, and sanitizer runs also passed before the candidate was pushed.

The randomized differential suite is implementation evidence, not an empirical proof of amortized complexity. Likewise, no red-black/AVL-style per-operation logarithmic guarantee is claimed.

## Promotion decision

A second amortized structure is not added simply to increase breadth. The phase-level architectural hypothesis — executable self-adjustment plus an honest separation of worst-case and amortized reasoning — is already represented coherently.

Phase 14 therefore moves to persistent/versioned data structures. Its first planned vertical slice is a persistent segment tree with immutable branching versions, `O(log n)` path copying, explicit structural-sharing evidence, checked signed range sums, and transactional failed updates that create neither a version nor unreachable arena nodes.
