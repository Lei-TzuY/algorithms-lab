# Phase 54 — dedicated frame-base physical-register reservation

## Executable boundary

Phase 51 sealed finite-register ownership for persistent allocation plus spill
scratch registers. Phase 52 turned spills into byte-addressed frame slots, and
Phase 53 sealed signed logical base-relative addressing. The remaining
cross-layer gap is that the logical frame base does not itself own a register
inside the finite physical-register file.

`plan_frame_base_reserved_backend` closes that ownership gap without changing
any sealed representation. For a caller-supplied physical-register count `R`:

- `R == 0` is an explicit infeasible hardware budget: no base register exists;
- for `R > 0`, physical id `R-1` is reserved deterministically as the frame base;
- sealed Phase 51 is rerun with exactly `R-1` non-base registers;
- every Phase-51 reservation attempt and its feasibility provenance is retained;
- when the non-base plan is feasible, sealed Phase 52 lays out the frame and
  sealed Phase 53 derives the requested base-relative displacements;
- the Phase-52 byte-addressed frame and Phase-53 addressed frame are both kept
  as replay witnesses;
- every physical-register reference in the final addressed frame is checked to
  be strictly below the dedicated base id.

The addressed frame therefore continues to report the non-base register-file
size (`R-1`), while the Phase-54 wrapper reports the total hardware register
count `R` and the dedicated base register separately. That distinction is part
of the contract rather than an implicit convention.

## Disjointness invariant

When `R > 0`, `base = R-1`. The sealed Phase-51 planner receives only `base`
register ids, so every persistent allocation and every materialized scratch
register is in `[0, base)`. Phase 52 and Phase 53 preserve those ids exactly.
Phase 54 also scans the addressed result before returning success and rejects an
internal inconsistency if any physical-register storage is not `< base`.

Thus persistent storage, spill-scratch storage, and the dedicated frame-base
register are pairwise disjoint by construction.

## Infeasibility and validation semantics

Register shortage is not an exception. The complete Phase-51 attempt sequence
is returned with `selection == nullopt`, and no frame is constructed. With
`R == 0`, no frame-base id exists and the Phase-51 zero-register plan is retained
for provenance.

Frame-layout and addressing configuration is evaluated only after a feasible
non-base selection exists. At that point all sealed Phase-52/53 validation and
checked-arithmetic errors propagate unchanged.

## Verification

Deterministic regressions cover zero/single-register hardware, a spill/scratch
case whose highest physical id is reserved for the base, non-base register-file
infeasibility, and propagation of Phase-52/53 configuration errors.

A fixed-seed 240-program corpus independently constructs the manual sealed
pipeline (`Phase51(R-1) -> Phase52 -> Phase53`) and requires byte-for-byte result
equality with the wrapper. Every successful result additionally scans all
class/location/operation physical-register references and every scratch witness
to prove they remain below the dedicated base id. Repeated planning is required
to be deterministic.

The exact merged-main repository matrix on
`main@17a17b56c09e875acebff93c5c3953b1e9e2fddc` (run `34418352836`)
completed successfully on GCC release, Clang release, and GCC ASan+UBSan. This
closes the integration gate for the Phase-54 ownership boundary.

## Complexity and non-claims

The wrapper performs one sealed Phase-51 planning invocation plus, when
feasible, one Phase-52 layout and one Phase-53 addressing pass. Its additional
ownership validation is linear in the returned storage/provenance records. No
stronger allocator or backend asymptotic claim is introduced.

This phase does **not** choose stack-growth direction, a concrete ISA opcode,
encoded displacement width, calling convention, ABI frame-pointer convention,
callee/caller-save policy, red zone, prologue/epilogue, variable-size objects,
or slot reuse/coloring. Reserving the highest id is a deterministic
machine-independent baseline, not an ABI prescription or a minimum-spill/
optimal-register-allocation claim.

## Sealed boundary and next frontier

Phase 54 is sealed after exact implementation CI, merge, merged-main CI, and the
architecture/integration audit recorded in `phase54_sealing_audit.md`.

The next coherent backend gap is the entry-time coordinate relation between the
now-owned frame-base register and the stack pointer around fixed-size frame
allocation. Phase 55 should add a target-neutral stack-direction-aware setup
without choosing an ISA opcode or ABI. For lower-growing storage it must express
`SP += -frame_size` and `base = adjusted_SP + anchor`; for higher-growing
storage it must express `SP += +frame_size` and
`base = adjusted_SP - (frame_size-anchor)`. The signed deltas must be checked,
and composition with every sealed Phase-53 displacement must recover the same
frame byte location.
