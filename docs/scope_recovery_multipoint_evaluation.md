# Scope recovery — prime-field multipoint polynomial evaluation

## Coverage decision

A fresh post-Phase-69 coverage audit after exact bounded Delaunay triangulation found no multipoint polynomial evaluation, subproduct tree, or remainder-tree implementation in live code, pull-request history, or the branch namespace. The occupied `scope-recovery-minimum-cycle-basis` and `scope-recovery-exact-treewidth` surfaces are intentionally untouched. The recovery authority remains `docs/scope_recovery_after_phase69.md`; stale compiler/backend ROADMAP headings remain frozen.

This slice changes proof model again rather than extending geometry or dynamic connectivity. It composes the sealed `998244353` NTT convolution and formal-power-series inverse into a divide-and-conquer polynomial evaluation index.

## Production contract

`multipoint_evaluate_mod_998244353(coefficients, points)` treats coefficients as little-endian and returns `P(points[i]) mod 998244353` in the original point order.

- coefficients and points are normalized modulo the prime field;
- empty point input returns an empty vector;
- the empty/zero polynomial returns zero at every point;
- duplicate points are legal and preserve duplicate output positions;
- existing NTT and FPS transform-size limits remain explicit representability limits rather than being hidden behind fallback semantics.

## Product-tree invariant

Each leaf stores the monic factor `x - x_i`. Each internal node stores exactly the product of its two child polynomials, so a node spanning point interval `I` stores `prod_{i in I}(x-x_i)`.

Evaluation first reduces the input polynomial modulo the root product, then recursively reduces each parent remainder modulo both child products. At a leaf, the remainder modulo `x-x_i` is the constant `P(x_i)` by the polynomial remainder theorem.

## Fast monic remainder

For dividend `A` and monic divisor `B`, let `k = deg(A)-deg(B)+1`. Reversing the high coefficients gives

`rev(Q) = rev(A) * inverse(rev(B)) mod x^k`,

where `Q` is the quotient. Because `B` is monic, `rev(B)` has constant coefficient one and is invertible as a formal power series. The implementation therefore reuses the sealed Newton/FPS inverse and NTT convolution rather than coefficient-by-coefficient long division. Reversing the recovered quotient and subtracting `Q*B` yields the degree-`< deg(B)` remainder.

The algebraic division identity and product-tree remainder theorem are proof obligations; differential tests are implementation evidence, not theorem proofs.

## Complexity / limits

Let `n` be the number of points, `d` the coefficient count, and `M(k)` the cost of the sealed NTT-backed polynomial multiplication at size `k`. Product-tree construction and remainder propagation use `O(M(n) log n)` convolution work; the initial reduction of a higher-degree polynomial contributes `O(M(d))`. Storage is `O(n log n)` coefficients in the direct resident product tree plus transient convolution/FPS workspace.

The implementation intentionally inherits the sealed NTT maximum transform size and FPS inverse-term limit. It does not claim arbitrary polynomial sizes, arbitrary moduli, Hermite/derivative evaluation, interpolation, or a general-purpose polynomial object model.

## Verification

Logic-focused development passed strict GCC, strict Clang, and actual ASan+UBSan against 500 random polynomial/point sets using an independent point-by-point Horner oracle. The pre-upload logic harness used naïve stand-ins for convolution/FPS with the same public contracts; therefore this evidence validates the product/remainder-tree logic but is not represented as repository-integration evidence.

The committed repo-native tests add deterministic duplicate-point, empty/zero, modular-normalization, and degree-greater-than-point-count cases plus 400 fixed-seed randomized differential cases against direct Horner evaluation. Exact full-repository GCC/Clang/ASan CI on the uploaded candidate remains the integration gate.
