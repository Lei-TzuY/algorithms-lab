# Phase 47 sealing audit

## Integrated checkpoint

Phase 47 closes the scalar out-of-SSA / phi-elimination boundary.

- implementation PR: #111, `Phase 47: out-of-SSA phi lowering`
- exact candidate head: `7d2640672f33d68aeb2fddc833c2adf3c9a4c7e8`
- exact candidate CI: run `34384957522`, GCC release / Clang release / GCC ASan+UBSan all successful
- integrated main: `e18833fe5777ce152022576503568c4dd53ee96b`
- merged-main CI: run `34385859699`, GCC release / Clang release / GCC ASan+UBSan all successful

## Capability audit

The phase now has one coherent executable boundary rather than a collection of
copy-placement cases:

- every retained scalar SSA identity becomes a virtual-register identity in a
  phi-free program;
- each logical predecessor / phi-block relation produces simultaneous copies
  with replayable lowering provenance;
- predecessor-exit and successor-entry placement cover unambiguous logical
  edges;
- every physical arc of a true critical logical edge is split independently, so
  parallel-edge multiplicity remains explicit;
- arbitrary parallel-copy bundles have a deterministic sequential schedule with
  fresh temporaries for cycles;
- malformed SSA/CFG structure is rejected rather than silently lowered.

The randomized copy-bundle oracle checks simultaneous semantics independently,
and the randomized CFG corpus independently recomputes placement class, split
multiplicity, logical lowering sites, and copy behavior. Candidate and
integrated-main compiler/sanitizer matrices are both green.

## Why Phase 47 stops here

Additional edge-shape regressions, alternative temporary-selection policies, or
copy-placement variants would exercise the same completed correctness model.
They would not add a distinct algorithmic capability and are therefore not a
reason to keep the phase open.

Register assignment is intentionally not treated as a small extension of SSA
destruction. It introduces liveness across the phi-free instruction/copy stream,
interference construction, a finite physical-register budget, and explicit spill
choices. Copy coalescing and spill rewriting add still further correctness
boundaries and are not prerequisites for the first allocation slice.

## Promotion

Phase 48 targets virtual-register allocation:

1. compute deterministic may-live state for every lowered block/program point;
2. build a reconstructable interference graph over `SsaValue` identities and
   out-of-SSA temporaries;
3. produce a deterministic assignment to a caller-supplied register budget;
4. report an explicit spill set when the chosen coloring heuristic cannot assign
   a register;
5. verify liveness/interference against an independent forward path-to-use oracle
   and verify every assigned interference edge has distinct registers.

The first slice makes no minimum-spill, optimal graph-coloring, copy-coalescing,
or spill-code insertion claim. Those remain separate hypotheses after the
allocation-analysis boundary is integrated and audited.
