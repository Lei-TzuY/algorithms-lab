# Phase 54 sealing audit

## Live checkpoint

Phase 54 reached `main@17a17b56c09e875acebff93c5c3953b1e9e2fddc` through PR #125. The exact merged-main CI run `34418352836` completed successfully on all repository gates: GCC release, Clang release, and GCC ASan+UBSan.

No implementation pull request or competing Phase-55 branch was open when this audit was prepared.

## Capability sealed

Phase 54 gives the logical frame base explicit ownership inside the same finite physical-register budget used by Phase 51. For a total hardware budget `R > 0`, register `R-1` is reserved deterministically as the frame base and Phase 51 is rerun over exactly `R-1` non-base registers. Feasible plans then reuse sealed Phase 52 byte layout and sealed Phase 53 base-relative addressing without changing their representations.

The wrapper retains the complete Phase-51 reservation-attempt provenance plus both the byte-addressed and base-relative frame witnesses. Before success it verifies that persistent physical-register storage, materialized spill-scratch registers, and every physical-register operand in the addressed program remain strictly below the dedicated base id.

`R == 0` is an explicit infeasible hardware budget with no frame-base id and no addressed frame. Register shortage remains a normal result rather than an exception, while sealed Phase-52/53 configuration and checked-arithmetic errors propagate unchanged after a feasible non-base selection exists.

## Evidence and invariant boundary

Deterministic regressions cover zero/single-register budgets, scratch/spill materialization below the reserved base, non-base infeasibility, and Phase-52/53 configuration error propagation. A fixed-seed 240-program corpus compares the wrapper byte-for-byte against the manually composed sealed pipeline `Phase51(R-1) -> Phase52 -> Phase53`; successful results additionally replay all physical-register references for base-register disjointness.

The repository matrix, not test timing, is the integration evidence. Phase 54 makes no register-allocation optimality, minimum-spill, ABI, ISA, or encoded-addressing claim.

## Architecture audit and next frontier

The finite-register ownership gap is now closed, but the dedicated base register is still only an owned identifier. The backend does not yet describe how that logical base relates to the stack pointer at function entry and after fixed-size frame allocation. That is the next cross-layer gap because Phase 53 already defines the logical byte anchor and Phase 54 already owns the physical base register.

Phase 55 should therefore add a target-neutral stack-direction-aware frame-entry setup derived from the sealed Phase-54 plan. For downward-growing storage, fixed-frame allocation moves the stack pointer by `-frame_size` and the base register points `+anchor` bytes from the adjusted stack pointer. For upward-growing storage, the stack pointer moves by `+frame_size` and the base register points `-(frame_size-anchor)` bytes from the adjusted stack pointer. The implementation must use checked signed arithmetic and prove that every Phase-53 frame displacement composes with the derived base delta to the same concrete frame byte offset.

This frontier deliberately stops before choosing a concrete stack-pointer register, ISA instruction, calling convention, callee/caller-save policy, red zone, prologue/epilogue encoding, or unwind metadata.

## ROADMAP presentation note

`ROADMAP.md` remains presentation-only historical state for this late backend sequence because the connected write surface does not expose a safe partial patch for that large file. The phase proof and sealing audit are authoritative for the Phase-54 seal and Phase-55 promotion; no production or correctness claim depends on a stale roadmap heading.
