# Phase 36 sealing audit

Phase 36 is sealed only after uniform represented-path assignment reached merged `main` and the exact merged-main CI matrix completed successfully.

## Integrated checkpoint

- implementation candidate: `11884b4d073bfe11fbb9834bf99d1f6d1cc2daca`;
- candidate CI run `34328960851`: GCC release, Clang release, and GCC ASan+UBSan all successful;
- integrated main commit: `7238fb95a16bdf4cb9b60bbcc2277691265c6e77`;
- merged-main CI run `34329511772`: GCC release, Clang release, and GCC ASan+UBSan all successful.

There were no open pull requests at sealing time, and no parallel Phase-36 seal or Phase-37 implementation branch was present.

## Correctness and lazy-state audit

The sealed extension adds a uniform represented-path assignment tag to `LinkCutForest` without changing represented-tree topology semantics, point assignment semantics, or exact public path-sum behavior.

Every path assignment, including a singleton path, uses the same represented-path exposure sequence: reroot the first endpoint, verify connectivity, access the second endpoint, then tag the exposed auxiliary tree. A prototype singleton shortcut was rejected after a deterministic regression demonstrated that `access(first)` alone can leave unrelated preferred-path vertices in the same auxiliary subtree and therefore over-assign them.

A pending assignment means the complete current auxiliary subtree is logically uniform. The tag composes with lazy reversal because reversal changes order but not membership or a uniform numeric value. Point assignment exposes the vertex first so inherited path assignments are materialized before the single-node overwrite.

The sealed Phase-35 two-limb signed-magnitude aggregate remains exact. Uniform assignment computes `assigned_value * auxiliary_size` without `__int128`, floating point, signed overflow, or intermediate `int64_t` narrowing. Public `path_sum()` continues to reject only genuinely out-of-range final mathematical results.

## Diagnostic invariant audit

`valid_auxiliary_invariants()` does not force lazy tags merely to inspect the structure. It carries inherited assignment state through the auxiliary forest while checking structural links, acyclicity, stored subtree sizes, tag-owning-node values, exact scaled aggregates, and the original recursive aggregate equality in untagged regions.

That distinction matters because descendants under a pending ancestor assignment may intentionally contain stale stored values until `push` materializes the logical update.

## Verification independence

The randomized differential is independent from the link-cut implementation. It maintains an adjacency-matrix represented forest, reconstructs unique paths with BFS, and performs path assignment eagerly. The fixed-seed trace executes 30,000 mixed operations over 40 vertices and interleaves `link`, `cut`, point assignment, path assignment, connectivity, path distance, and path sum while checking auxiliary invariants after every operation.

Deterministic tests cover overlapping assignment order, point overwrite after deferred assignment, topology changes with pending tags, singleton assignment, disconnected/bounds rejection, cancellation, positive/negative public overflow, and exact `INT64_MIN`/`INT64_MAX` boundaries.

## Complexity claim audit

A represented-path assignment performs one ordinary link-cut path exposure plus constant-time tag application. It therefore preserves the standard amortized `O(log V)` operation bound over a sequence and `O(V)` resident storage. One individual operation may still take `O(V)` before amortization; no worst-case-per-operation logarithmic claim is made.

No CI timing or uncontrolled benchmark is used as asymptotic evidence.

## Scope decision

Phase 36 is complete and should not absorb path addition, min/max, or a generic affine-tag abstraction merely because they use the same auxiliary trees. Path addition introduces a new composition law with assignment and also a new per-node representability problem: adding a signed delta must be rejected transactionally if any affected node would leave the public `int64_t` value domain, even when the aggregate sum itself remains representable.

That obligation is materially different from uniform assignment and justifies promotion rather than variant farming.

## Promotion

Phase 37 promotes to **affine lazy represented-path updates**. The first slice adds uniform path addition while preserving dynamic topology, point assignment, sealed path assignment, exact path sums, and lazy reversal.

The implementation must maintain enough exposed-path value-range information to prove an addition is safe for every affected node before mutation, compose addition after/before pending assignment correctly, update the wide path aggregate by an exact `delta * auxiliary_size`, and keep failure transactional. Verification must include assignment/addition overwrite-order cases, `INT64_MIN`/`INT64_MAX` per-node boundaries, aggregate cancellation, topology changes under pending tags, and long randomized traces against an eager naïve forest/BFS oracle.
