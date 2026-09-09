# Phase 47 — out-of-SSA lowering and phi elimination

## Scope and contract

Phase 47 consumes a validated Phase-45/46 `SsaProgram` together with the directed
CFG from which it was constructed and emits a phi-free virtual-register program.

The lowering deliberately preserves `SsaValue` identities as virtual registers.
This is SSA destruction, not register allocation or coalescing: an original SSA
definition remains a distinct virtual location, while each phi result is defined
by edge-specific copies from its predecessor value.

The result contains:

- a rewritten directed CFG;
- original blocks with their non-phi `SsaInstruction` sequence unchanged;
- any split blocks introduced for physical critical edges;
- deterministic entry/exit copy schedules attached to the block where they execute;
- one logical lowering record per predecessor-block / phi-block pair;
- the original simultaneous parallel-copy bundle and its sequentialized move list;
- a global temporary namespace used only when scheduling a cyclic parallel-copy
  bundle.

No phi node survives in the result type.

## Placement and critical-edge invariant

Phi incoming witnesses in the sealed SSA format are keyed by unique predecessor
block even when the `Graph` contains parallel physical arcs. Phase 47 therefore
first reasons about unique reachable predecessor/successor block relations.

For a non-empty copy bundle on logical edge `p -> s`:

1. if `p` has one unique reachable successor, the schedule executes at `p` exit;
2. otherwise, if `s` has one unique reachable predecessor, it executes at `s`
   entry;
3. otherwise the logical edge is critical and every physical `p -> s` arc is
   split independently.

Splitting each physical parallel arc preserves its multiplicity. The original
edge weight is retained on `p -> split`; the synthetic `split -> s` edge has
weight zero because CFG weights are outside the sealed SSA semantics. Every path
that traversed one original physical arc now traverses exactly one corresponding
split block and executes the same logical phi-copy bundle exactly once.

No split is created for an edge whose successor has no effective phi copy.

## Parallel-copy semantics

`SsaParallelCopy{destination, source}` is simultaneous. A general first-principles
scheduler is exposed separately because a future allocation/coalescing layer can
produce cyclic location dependencies even though the current scalar SSA
construction normally keeps different variables in distinct value identities.

The scheduler:

- rejects duplicate destinations;
- removes exact self-copies;
- repeatedly emits a copy whose destination is no longer needed as a source;
- when only cycles remain, saves the lexicographically smallest remaining
  destination to a fresh temporary, substitutes that temporary for subsequent
  reads of the saved value, and continues.

This is a direct sequentialization of parallel-copy semantics. The temporary
namespace is deterministic and local schedules are offset into one program-wide
namespace during CFG lowering.

The direct implementation is intentionally simple and may perform quadratic work
in the number of copies in one bundle; no optimized copy-coalescing bound is
claimed.

## Validation

`destroy_ssa` validates the structural contract it relies on rather than silently
accepting an arbitrary malformed `SsaProgram`:

- the CFG is directed and `program.start` is valid and predecessor-free;
- program block reachability equals CFG reachability;
- unreachable blocks contain no SSA payload;
- version-zero initial values cover every variable exactly;
- SSA definitions are unique;
- every value refers to a valid variable and every use/incoming value is defined;
- at most one phi exists per variable per block;
- each phi result and incoming value has the phi variable;
- each phi contains exactly the unique reachable predecessor-block set.

The lowering does not re-prove that the supplied SSA placement is minimal/pruned
or that renaming itself is semantically correct. Those properties belong to the
sealed Phase-45/46 construction contracts.

## Verification

Deterministic regressions cover:

- cycle-safe generic parallel-copy scheduling with a fresh temporary;
- duplicate-destination rejection;
- predecessor-exit placement;
- successor-entry placement;
- a true critical logical edge with two physical parallel arcs, requiring two
  split blocks while keeping one logical phi-copy witness;
- malformed undirected, entry-predecessor, and phi-predecessor shapes.

Randomized verification contains two independent corpora:

- 1,500 fixed-seed arbitrary permutation/cycle parallel-copy bundles are executed
  against a snapshot-based simultaneous-copy oracle;
- 450 fixed-seed reachable forward DAG CFGs with 2–9 blocks, 1–4 variables,
  random joins, branches, parallel physical arcs, and synthetic valid phi
  witnesses are lowered. The tests independently recompute unique CFG degrees,
  expected placement class, physical split multiplicity, one-site-per-logical
  predecessor/phi-block identity, and simultaneous-copy semantics.

Focused strict GCC, strict Clang, and real ASan+UBSan builds pass before upload.
The exact-head repository GCC/Clang/ASan matrix remains the authoritative
integration gate.

## Complexity boundary

Let `V` and `E` be CFG vertices/physical arcs, `P` the total phi incoming-copy
payload, and let `c_i` be the size of lowering bundle `i`.

The direct implementation uses:

- `O(V + E)` reachability and unique predecessor/successor construction, apart
  from deterministic list sorting;
- `O(P)` logical bundle extraction plus sorting of each bundle;
- `O(sum c_i^2)` worst-case parallel-copy scheduling;
- `O(V + E + S + P)` resident lowering state, where `S` is the number of
  synthetic split blocks.

No register-allocation/coalescing, MemorySSA, optimization-pass, or
compiler-wide complexity claim is imported.

## Current boundary

Phase 47 is scalar virtual-register SSA destruction. Register assignment,
coalescing, spill code, MemorySSA, optimization passes, post-dominators/control
dependence, and incremental CFG maintenance remain separate architectural
hypotheses.
