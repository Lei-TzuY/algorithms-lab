# Phase 42 Sealing Audit

## Integrated checkpoint

Phase 42 is sealed only after the directed minimum-arborescence implementation reached merged `main` and the exact merged-main CI gate completed successfully.

- implementation PR: #101, `Phase 42: directed minimum arborescence`
- exact implementation head: `9dd4b6ed954a684dc2013fd83eff7ba8f26cb6e5`
- implementation base: `63db47c67a5c68b288c6c59e4c267787fde3c735`
- merged main checkpoint: `34c288d206d0d7595bf7c216289b8592ba76de04`
- merged-main CI: run `34363518566`, completed successfully on GCC release, Clang release, and GCC ASan+UBSan

The implementation is therefore not sealed from PR-level evidence alone.

## Correctness and architecture audit

The source-level audit confirmed that the merged implementation contains the intended first-principles Chu-Liu-Edmonds machinery rather than a wrapper around a library solver:

- deterministic minimum-incoming selection with original-edge-index tie breaking;
- simultaneous directed-cycle contraction and recursive solution of the contracted graph;
- adjusted entering-edge costs that measure replacement cost relative to the selected incoming baseline;
- expansion through per-level parent-edge provenance so nested contractions reconstruct original input edge indices;
- explicit root/endpoint validation and rejection when the root cannot reach every vertex;
- parallel-edge preservation and self-loop exclusion from the optimization;
- exact signed internal arithmetic whose range can exceed `int64_t`, with narrowing only for the final public optimum.

The public witness is replayable: successful results contain exactly `V-1` original input edges, one incoming edge for every non-root vertex, no incoming edge for the root, root reachability over the selected edges, and an exact total equal to the returned cost.

## Independent verification boundary

Deterministic regressions cover acyclic inputs, ties, parallel edges, self-loops, direct and nested contractions, invalid endpoints/root, unreachable instances, representable final results whose adjusted intermediate costs leave the signed-64-bit range, and positive/negative final-total overflow.

The fixed-seed differential suite checks 600 reachable directed signed-weight multigraphs with 2-6 vertices against an independent exhaustive rooted-arborescence oracle. The oracle enumerates original incoming-edge choices and validates rooted reachability directly; it does not reuse Chu-Liu-Edmonds contraction or expansion.

These tests are implementation evidence. The Chu-Liu-Edmonds contraction theorem remains the mathematical proof obligation and is not inferred from the finite corpus.

## Complexity and claim audit

The merged code states the conservative direct-implementation bounds `O(VE)` time and `O(VE)` materialized/recursive auxiliary space. It does not claim a heap-optimized or asymptotically improved arborescence implementation, and CI timing is not used as performance evidence.

No correctness, sanitizer, representability, integration, or complexity-claim blocker remains at this checkpoint.

## Why Phase 42 stops here

Adding alternate arborescence implementations, minor tie-policy variants, or a faster implementation solely to keep the phase open would mostly optimize or duplicate an already exact capability without introducing a comparably new proof boundary. Phase 42 therefore seals after one coherent implementation instead of farming variants.

## Promotion rationale

The next uncovered directed-graph frontier is control-flow dominance. Repository and PR search found no dominator-tree implementation or existing Phase-43 work.

Phase 43 should introduce exact dominator trees from a chosen start vertex, including reachable/unreachable semantics, immediate-dominator witnesses, deterministic directed-graph handling, and a first-principles semi-dominator/link-eval construction. Small graphs should be checked against an independent iterative dominator-set fixed-point oracle rather than another semi-dominator implementation. This adds genuinely new graph proof machinery instead of extending the arborescence surface sideways.
