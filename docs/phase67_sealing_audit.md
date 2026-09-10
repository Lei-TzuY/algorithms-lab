# Phase 67 sealing audit

## Verified checkpoint

Phase 67 was implemented by PR #151. Its exact candidate
`dc1dc7f281914ca4ff3af5d19992549d213286f3` passed CI run `34486863075`
on GCC release, Clang release, and GCC ASan+UBSan before squash integration.
The merged checkpoint is `615cf49baf85502e18eef7aa95951767e99a6a77`;
merged-main CI run `34488175900` also completed successfully on all three jobs.

## Architecture findings

The phase closes one coherent semantic boundary rather than adding an alternate
executor. Canonical scalar semantics survive SSA destruction, spill lowering,
scratch-register physicalization, byte frame layout, and base-relative
addressing before the existing continuation executor consumes them. Known scalar
instructions derive their results from actual materialized register inputs while
opaque instructions preserve the sealed caller-reply and honest-suspension
contract.

No unresolved correctness blocker was found in descriptor shape validation,
checked arithmetic, transfer-opacity validation, reply ordering, or provenance
retention. Phase-64 path continuation and Phase-65 exit-event execution inherit
the same block executor rather than duplicating scalar semantics.

## Remaining semantic boundary

The retained program representation still carries only CFG topology for control
flow. `SsaBlock` / `SsaLoweredBlock` do not contain an executable terminator or
branch descriptor, and the current CFG-path executor intentionally receives the
visited path from its caller. Therefore choosing a successor from adjacency
order, out-degree, or block numbering would manufacture semantics that the IR
does not contain.

Phase 68 should introduce one explicit, validated control/terminator semantic
boundary and carry it through the same ownership/provenance pipeline far enough
for executable successor selection. Predicate uses must participate in the real
SSA/liveness/allocation path rather than being attached after allocation. Opaque
control must remain caller-owned. Termination, general memory, calls, ABI, and
target ISA semantics remain separate frontiers unless explicitly represented.

## Scope decision

Phase 67 is sealed here. Adding more scalar arithmetic opcodes, more caller-path
variants, or documentation-only aliases would refine existing behavior without
adding a new architectural capability. The next work must cross the explicit
control-semantic boundary instead.
