# Phase 53 sealing audit

## Live checkpoint

Phase 53 reached `main@7c1c1dfc2440e1c02009fa2a1ce7672140143b8e` through PR #123. The exact merged-main CI run `34415763955` completed successfully on all repository gates: GCC release, Clang release, and GCC ASan+UBSan.

No implementation pull request or competing Phase-54 branch was open when this audit was performed.

## Capability sealed

The Phase-52 byte-addressed frame now has a separate target-neutral base-relative view. Every frame slot preserves its slot id, source byte offset, byte size, and gains one checked signed displacement derived exactly as `slot_offset - logical_base_anchor`. Class storage, original-location storage, reload/store operands, operation provenance, block reachability, original-block provenance, and all physical-register ids are preserved.

The caller owns both the logical anchor and inclusive displacement policy. The implementation validates the complete Phase-52 frame contract and rejects malformed slot/register witnesses, non-slot byte references, out-of-frame anchors, signed representability overflow, or policy-excluded displacements. The exactly representable `INT64_MIN` case remains supported.

## Evidence and invariant boundary

The deterministic regression suite covers positive/negative addressing, zero-frame behavior, invalid policies, malformed Phase-52 evidence, and signed-arithmetic boundaries. A fixed-seed 350-program corpus replays the real Phase-49/50/51/52 pipeline and independently checks slot, class, location, block, operation, storage, and provenance mappings. Repeated construction for one source/configuration is deterministic.

The repository matrix is the authoritative integration evidence. No test or CI timing is used as a performance claim.

## Architecture audit

Phase 53 closes the representation gap from absolute frame-byte offsets to checked signed base-relative offsets, but intentionally leaves the logical base without ownership of a physical register. That omission is now the highest-value cross-layer gap: Phase 51 proves persistent and spill-scratch physical registers are disjoint, while Phase 53 assumes an external logical base that is not represented in the finite register file.

The next phase should therefore reserve one dedicated frame-base physical register before Phase-51 planning. A deterministic baseline can reserve the highest register id, run the sealed Phase-51 planner over the remaining `R-1` registers, and—only when that plan is feasible—reuse sealed Phase 52 layout and Phase 53 addressing unchanged. The result must expose the full Phase-51 attempt provenance plus the addressed frame and prove no persistent or scratch storage ever aliases the dedicated base register.

This is a stronger integration step than choosing an ISA opcode or ABI prematurely: it closes finite-register ownership while keeping target policy explicit.

## Non-claims

The sealed phase and the promoted frontier do not choose stack growth, a concrete base/frame pointer convention, load/store opcode, encoded displacement width, calling convention, red zone, callee/caller-save policy, prologue/epilogue, variable-size objects, slot reuse/coloring, or ABI-minimal frame shape. They also make no global register-allocation, minimum-spill, or backend-optimality claim.

## ROADMAP presentation note

`ROADMAP.md` still displays Phase 53 as `ACTIVE FRONTIER` because the connected GitHub write surface does not expose a safe partial patch for that large historical file and byte-exact reconstruction was intentionally not risked during this seal. This is presentation-only drift: this proof and audit are the authoritative Phase-53 seal evidence. The roadmap should be repaired when a safe partial-file patch surface is available; no production or correctness claim depends on the stale heading.
