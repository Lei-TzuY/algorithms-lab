# Phase 34 sealing audit

Phase 34 is sealed after the first-principles link-cut dynamic-forest implementation reached the exact integrated `main` tree and the merged-main CI matrix passed.

## Integration evidence

- implementation candidate head: `9c2023ca7393cd19d3e7392e08a4534165ee0ccc`;
- exact candidate tree: `94d47ebd22873c388b2b5751c9c9e783d7faaab1`;
- candidate CI run `34320751938`: GCC release, Clang release, and GCC ASan+UBSan all successful;
- GitHub's normal squash endpoint became stuck in `merge already in progress`; the unchanged exact candidate tree was therefore integrated as one fast-forward commit with parent equal to the unchanged main base;
- integrated main commit: `234984b29de44d546fba6c07598e9231e1e33a7b`;
- merged-main CI run `34321406201`: GCC release, Clang release, and GCC ASan+UBSan all successful.

The fallback changed no candidate bytes and did not bypass candidate or merged-main verification.

## Correctness boundary

The sealed capability is dynamic topology over a simple undirected forest:

- online `link` between different represented trees;
- exact direct-edge `cut`;
- connectivity;
- exact represented-path edge distance;
- explicit distinction between represented-tree topology and the auxiliary preferred-path splay forest;
- lazy evert/reversal and first-principles access/splay machinery;
- executable auxiliary child/parent, acyclicity, coverage, and subtree-size invariants.

The long randomized differential uses an independent adjacency-matrix forest plus BFS; it does not replay link-cut operations internally. Rejection cases cover cycle-creating links, self-links, non-edge cuts, disconnected path queries, and invalid vertices.

## Complexity claim audit

The implementation claims the standard link-cut amortized `O(log V)` bound over an operation sequence and `O(V)` resident storage. It explicitly does not claim worst-case `O(log V)` for an individual operation: a single auxiliary splay path can be linear before amortization.

No benchmark timing is used as asymptotic evidence.

## Scope decision

Path sums/min/max, node or edge weights, subtree aggregates, dynamic vertex creation, and parallel represented edges remain outside Phase 34. Adding aggregate values changes both auxiliary-state invariants and representability obligations, so treating it as one more Phase-34 query would blur the sealed topology boundary.

Phase 34 is therefore complete rather than extended with nearby feature variants.

## Promotion

Phase 35 promotes to **augmented dynamic trees**. The first coherent slice adds signed node values, point assignment, and exact represented-path sums to the sealed link-cut forest. Auxiliary aggregates must remain exact through access, rotations, and lazy reversal; public `int64_t` results may reject only genuinely out-of-range exact sums. Verification will mix topology and value mutations against an independent naïve forest/BFS path-sum oracle and include overflow/cancellation regressions.
