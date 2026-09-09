# Phase 40 sealing audit

Phase 40 is sealed only after the rooted represented-subtree numeric aggregation implementation reached merged `main` and the exact merged-main CI matrix completed successfully.

## Integration evidence

- implementation PR #97 targeted the active Phase-40 frontier and intentionally left `ROADMAP.md` unchecked until merged-main verification;
- merged main checkpoint: `9d06a7831a3798723c594d0a6d64b79e22ded1e3`;
- merged-main CI run `34348678771` completed successfully on GCC release, Clang release, and GCC ASan+UBSan;
- the merged tree is byte-identical to the final Phase-40 branch tree `a070d611326d330f003c71641dde20e03d76df90`.

## Capability audit

Phase 40 closes the numeric analogue of the Phase-39 represented-cardinality invariant rather than adding a cosmetic query alias.

The link-cut forest now separates:

- auxiliary-path node-value sum;
- immediate virtual-child represented sum;
- aggregate virtual contribution across an auxiliary splay.

`access` transfers exact represented numeric contribution when preferred and virtual children exchange roles. `link` and `cut` preserve the same represented-value invariant. Point assignment, represented-path assignment, and represented-path addition remain strictly auxiliary-path-local and therefore do not accidentally modify virtual descendants.

After reroot/access, `rooted_subtree_sum(root, vertex)` subtracts the exposed ancestor-side represented contribution and narrows only the final exact result to `int64_t`.

## Verification audit

The dedicated Phase-40 test surface includes:

- deterministic rerooting and branching cases;
- lazy represented-path assignment/addition while virtual descendants exist;
- cut/relink and disconnected-query rejection;
- positive/negative cancellation and exact overflow boundaries;
- a 6,000-step fixed-seed mixed dynamic-forest trace against an independent eager adjacency/value oracle;
- final all-pairs rooted-subtree-sum comparison;
- structural diagnostics that independently reconstruct and verify virtual cardinality and exact virtual numeric contribution.

This evidence is independent of the link-cut implementation's own query recurrence.

## Claim boundary

Phase 40 preserves the standard link-cut sequence-level amortized `O(log V)` operation claim with `O(V)` resident state. It does not claim worst-case `O(log V)` for each individual access/splay, and CI wall-clock time is not used as asymptotic evidence.

## Frontier decision

The dynamic-forest path/rooted-subtree line is now a mature checkpoint. Adding `subtree_min`, `subtree_max`, or another aggregate with the same virtual-accounting pattern would mostly repeat an already demonstrated invariant rather than open a new architectural surface.

The next promoted frontier is therefore an exact global cut structure: a Gomory-Hu tree for undirected non-negative-capacity multigraphs. It should reuse the sealed Dinic implementation for `V-1` flow computations, expose a deterministic cut-equivalent tree and pairwise min-cut queries, and verify small instances against an independent exhaustive s-t cut oracle. The Gomory-Hu cut-equivalence theorem remains a mathematical proof obligation rather than something inferred from randomized tests.