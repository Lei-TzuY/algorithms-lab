# Phase 28 Sealing Audit

## Integrated checkpoint

Phase 28 implementation PR #73 merged as `main@b24ff6f3d9a6350423b24ec2b37cac7ca872487a`. The exact merged-main CI run `34186632542` completed successfully on GCC release, Clang release, and GCC ASan+UBSan, including full Build/Test steps.

## Correctness audit

The affine-gap state machine represents all scoring information that changes the cost of the next transition. In particular, insertion/deletion gap mode is part of the domination key; pattern progress, candidate length, and both paired BWT intervals remain part of state identity; only the least score for an equivalent full scoring state dominates later arrivals.

All non-match costs are positive. This makes Dijkstra ordering well-founded and keeps a bounded-score search finite. Match/substitution reset gap mode; continuing the same gap pays `gap_extend`; switching gap type opens a new run. The full-pattern deletion fast path is valid because it constructs the empty substring as a witness at every text boundary exactly when one affine deletion run fits the score budget.

Every text-consuming transition remains on the sealed bidirectional BWT extension surface. Terminal positions are reconstructed through sealed exact BWT locate rather than a production direct scan.

## Verification independence

The primary randomized oracle is a separate three-matrix affine-gap dynamic program over direct substrings. It does not call BWT, suffix-array, the Phase-27 searcher, or the Phase-28 frontier. A secondary cross-phase property verifies that costs `{1,1,1}` produce exactly the sealed Phase-27 Levenshtein position set.

Deterministic tests additionally cover insertion/deletion runs, gap switching, arbitrary bytes, invalid zero costs, representability boundaries, full-deletion semantics, and replayable diagnostics.

## Complexity and claim audit

The implementation deliberately remains a first-principles exhaustive bounded search. Reachable state count can grow exponentially with score budget and byte branching; each BWT extension, heap operation, domination-map operation, and terminal locate is accounted for rather than hidden behind a polynomial-time or compressed-optimal claim.

No seed-and-extend, bit-parallel, probabilistic, affine-alignment acceleration, r-index-optimal, or universal performance claim is justified by this phase.

## Coverage decision

Adding another edit-cost variant would now be low-value breadth. The next structural blocker is different: exact candidate verification cannot become a genuine seed-and-verify architecture while the compressed index has no public way to recover bounded source text without falling back to a retained source string.

Phase 29 therefore promotes BWT text reconstruction and validated bounded extraction. The first baseline may reconstruct linearly and expose that cost honestly; its purpose is to establish the correctness substrate for later exact candidate-generation/verification work, not to claim optimal compressed extraction.

## Seal result

No unresolved production-correctness, integration, oracle-independence, or claim-boundary blocker remains. Phase 28 is sealed and Phase 29 is the active frontier.
