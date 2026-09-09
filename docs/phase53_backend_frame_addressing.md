# Phase 53 — target-neutral base-relative frame addressing

## Executable boundary

Phase 52 sealed deterministic byte offsets for every abstract spill slot while
preserving physical-register ids and backend provenance. Phase 53 adds the next
representation layer without mutating that sealed frame. The caller supplies a
logical frame-base byte anchor and an inclusive signed displacement policy.
`address_scratch_aware_backend_frame` produces a separate view in which:

- every physical-register id is preserved exactly;
- every Phase-52 frame slot retains its dense slot id, source byte offset, and
  byte size while gaining one signed `int64_t` base-relative displacement;
- every class, source-location, reload/store input, and output that referenced a
  Phase-52 frame byte offset uses that same derived displacement;
- operation kind/origin, block reachability, and original-block provenance are
  preserved exactly;
- the result contains only physical-register storage or signed frame-base
  displacement storage.

The logical anchor is measured from the Phase-52 frame origin and may be any
byte position in `[0, frame_size_bytes]`. Anchor zero therefore supports
positive displacements, while an anchor at frame end supports negative
displacements without choosing a stack-growth direction.

## Checked signed arithmetic and policy

For each slot start `offset`, production computes exactly

`displacement = offset - frame_base_byte_anchor`.

The subtraction is performed by unsigned magnitude comparison first; it does
not cast potentially large `size_t` values to signed arithmetic prematurely.
Positive magnitudes above `INT64_MAX` and negative magnitudes above
`INT64_MAX+1` are rejected. The exactly representable `INT64_MIN` case is
accepted explicitly.

After representability is proven, every slot displacement must lie in the
caller-provided inclusive `[minimum_displacement, maximum_displacement]` range.
An inverted range, anchor outside the frame, or policy-excluded displacement is
rejected rather than truncated or wrapped.

## Source-frame validation

Phase 53 treats the Phase-52 object as a typed boundary but still rejects a
manually corrupted frame before deriving addresses. It independently replays:

- slot-size/alignment and whole-frame alignment validity;
- dense deterministic slot ids, offsets, byte sizes, and aligned frame size;
- total/allocatable/reserved physical-register counts and scratch-register
  witness ordering;
- persistent physical-register references against the allocatable prefix;
- operation physical-register references against the full register file;
- every frame-byte reference against an actual Phase-52 slot start.

This is consistency validation, not a second frame-layout optimizer.

## Verification

Deterministic regressions cover positive and negative addressing, zero-size
frames, inverted policies, out-of-frame anchors, policy rejection, malformed
Phase-52 slot/register metadata, exact `INT64_MIN`, and positive/negative signed
representability overflow.

A fixed-seed 350-program corpus reuses the real Phase-49/50/51/52 pipeline. For
every feasible backend selection it chooses bounded frame parameters plus a
logical anchor, constructs the Phase-53 view twice, and independently replays
every slot/class/location/block/operation mapping and provenance field.

The repository GCC/Clang/ASan+UBSan matrix is the authoritative integration
gate. Local focused source arithmetic tests are supplementary evidence only.

## Complexity and non-claims

For `S` frame slots and `N` total storage references, construction is
`O(S + N log S)` with the current binary-search mapping from validated byte
offsets to slot displacements, and `O(S + N)` result storage. Because Phase-52
slot offsets are sorted, a future direct map could reduce lookup overhead, but
this phase makes no performance claim beyond the implementation above.

This phase does **not** choose a concrete frame/base physical register, stack
growth convention, machine load/store opcode, encoded displacement width,
addressing-mode syntax, calling convention, red zone, callee/caller-save
policy, prologue/epilogue, or ABI. The signed displacement interval is explicit
caller policy, not an ISA claim.

## Phase boundary

Phase 53 remains an implementation frontier until the exact candidate reaches
merged `main` and the merged-main CI matrix succeeds. Any next backend phase
must be selected only after that architecture/integration audit.
