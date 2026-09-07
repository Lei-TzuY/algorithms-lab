# Phase 7 sealing audit

Phase 7 is sealed only after all three ordered implementation slices were merged and the exact integrated checkpoint passed the repository's full CI matrix.

## Integrated checkpoint

- integrated main: `b716098404476e41219271e1a48a5409768c7356`
- merged-main CI: run `34071276566`, completed / success
- GCC Release: success
- Clang Release: success
- GCC ASan+UBSan: success
- no open implementation PR existed when the sealing branch was created

## Capability audit

### Computational geometry

The geometry surface is deliberately bounded instead of pretending to solve robust floating-point geometry generally. Coordinates outside `[-1e9, 1e9]` are rejected. Inside that domain, coordinate differences are at most `2e9`, determinant products at most `4e18`, and the determinant difference at most `8e18`, which remains representable in signed `int64_t`.

Andrew monotone-chain construction has a deterministic hull contract. Its randomized optimum/shape evidence uses a structurally independent Jarvis-march oracle, while segment-intersection symmetry tests are correctly described only as property checks.

### Number theory

The modular core avoids overflowing `uint64_t` products through modular addition/doubling and routes exponentiation through that multiplier. Large known-answer vectors were computed independently with arbitrary-precision arithmetic; randomized small-modulus tests use direct multiplication only in a domain where the oracle itself cannot overflow.

Full-domain deterministic Miller-Rabin classification explicitly depends on the documented seven-witness theorem. The tests exercise the implementation and pseudoprime regressions but do not claim to prove that external theorem. The API is not described as cryptographic and no constant-factor performance claim is made.

### Heavy-light decomposition integration

HLD is a real cross-phase integration: it consumes the existing undirected `Graph` and materializes its linearized vertex state in the Phase-4 checked `SegmentTree`. Strict-tree validation precedes state exposure; point updates inherit transactional segment-tree semantics; path accumulation adds an explicit checked boundary across chain ranges.

The heavy/light layout establishes contiguous heavy chains and contiguous rooted subtrees. The standard halving argument bounds light-edge crossings by `O(log V)`, yielding `O(log^2 V)` path sums and `O(log V)` point/subtree operations. Randomized verification uses BFS/path reconstruction and rooted-parent descendant scans that do not reuse the production decomposition.

## Cross-phase and oracle audit

- Geometry adds an exact-predicate domain rather than duplicating graph/DP surfaces.
- Number theory supplies portable modular exponentiation that can be reused by the promoted transform frontier.
- HLD connects Phase-1 graph representation with Phase-4 range-query state rather than introducing a parallel range tree.
- Randomized oracles are structurally separate from the production recurrence/decomposition in all three slices.
- CI exercises the complete repository under two release compilers plus ASan/UBSan, including the exact merged Phase-7 checkpoint.

## Validation and claim boundaries

No correctness, integration, oracle-independence, representability, or complexity-claim blocker was found. Important limits remain explicit: bounded geometry coordinates, external Miller-Rabin witness theorem, fixed-width arithmetic, HLD's strict simple-tree input, SegmentTree representability constraints, and dense graph vertex IDs.

Strict-tree validation logic is currently duplicated across LCA, tree DP, and HLD. That is a maintainability/architecture cleanup opportunity, not a correctness blocker: each public surface enforces its own contract and has adversarial validation coverage. Consolidating the shared rooted-tree validation/index layer should be considered when a later feature needs the same substrate, rather than forced into this sealing change.

## Decision and promotion

Phase 7 is **SEALED**. Further Phase-7 corner-case farming would have lower value than moving the architecture frontier.

Phase 8 is promoted to **algebraic transforms and polynomial algorithms**. The first executable slice is a radix-2 Number Theoretic Transform and polynomial convolution over `998244353`. It should reuse the Phase-7 modular exponentiation for roots/inverses, use direct `uint64_t` butterfly multiplication only where the fixed modulus makes the product representable, enforce the transform's power-of-two and `2^23` length boundary, and compare randomized convolutions against an independent naïve modular oracle. No benchmark or generalized exact-convolution claim is made by promotion alone.
