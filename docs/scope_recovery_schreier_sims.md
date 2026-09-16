# Scope recovery: deterministic Schreier-Sims permutation groups

## Coverage decision

A fresh live-state audit at `main@6d646bc973580822d0184a93c43717a0889afe3c`
after the merged Robinson-Schensted checkpoint found no Schreier-Sims,
stabilizer-chain, or finite permutation-group production surface in default-branch
code, pull-request history, or branch names.  The repository already contains
several neighboring combinatorial capabilities, but none answers subgroup
membership or exact generated-group order from permutation generators.

This slice follows `docs/scope_recovery_after_phase69.md`.  The frozen Phase-45--69
compiler/backend surface and the historically stale `ROADMAP.md` presentation are
left untouched.  The proof model also deliberately changes from the immediately
preceding bijective/tableau and alphabetic-coding work to orbit-stabilizer group
action.

## Production contract

`SchreierSimsGroup(degree, generators)` accepts generators that are exact
bijections of `[0, degree)`.  Invalid size, duplicate images, or out-of-range
images reject with `std::invalid_argument`.

The implementation canonicalizes the supplied generators together with their
inverses, then uses base points `0,1,...,degree-1`.  For each level it computes:

- the current stabilizer orbit of the base point,
- deterministic transversal representatives mapping the base point to every
  orbit point,
- Schreier generators for the next point stabilizer.

The public level diagnostic exposes the base point, sorted orbit, and normalized
next-stabilizer generator count.  `order_factors()` returns the exact
orbit-stabilizer factors even when their product is too large for `uint64_t`;
`order_u64()` checks multiplication and throws `std::overflow_error` rather than
silently wrapping.

`contains` performs exact subgroup membership by sifting.  `sift` additionally
returns the first level at which the residue image is outside the stored orbit
and the unreduced residue itself.  Successful membership ends with the identity
residue, making both positive and negative decisions replayable from the chain.

## Correctness boundary

For a level with stabilizer `G_i`, base point `b_i`, and transversal `t_alpha`
with `t_alpha(b_i)=alpha`, each generated Schreier element

`h = t_beta^{-1} * s * t_alpha`, where `beta=s(alpha)`,

fixes `b_i`.  Schreier's lemma states that these elements generate the next
stabilizer.  Recursing over the full base therefore yields an exact stabilizer
chain.  Orbit-stabilizer gives

`|G| = product_i |Orb_{G_i}(b_i)|`.

Membership sifting uses the same transversals in the reverse direction: after
levels `0..i-1`, the residue fixes all earlier base points; if its image of
`b_i` is absent from that orbit, the original permutation cannot belong to the
group.  Otherwise multiplying by the inverse transversal fixes `b_i` and
continues.

These are mathematical proof obligations.  Finite differential tests provide
implementation evidence; they are not a proof of Schreier's lemma.

## Verification

Focused exact candidate bytes pass:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic cases cover the trivial group, cyclic `C5`, square dihedral `D4`,
full `S4`, malformed generators, successful and failed membership residue replay,
and exact `S21` orbit factors with deliberate `uint64_t` order overflow.

The primary independent oracle never uses stabilizer chains.  For 180 fixed-seed
generator sets of degree `0..6`, it breadth-first enumerates the full generated
group from the identity.  Every one of the `n!` permutations is then checked for
membership equality, and the enumerated group cardinality must equal the
Schreier-Sims order.  Reversing the generator input order must leave the complete
orbit-factor sequence unchanged.

## Complexity and non-claims

This is a direct educational deterministic Schreier-Sims baseline, not a tuned
computational-group package.  With degree `n`, a level explores at most `n`
orbit points and forms one Schreier candidate per orbit-point/generator pair;
permutation composition/inversion costs `O(n)`.  Generator sets are normalized
and deduplicated lexicographically after each level.  The resulting practical
cost therefore depends on intermediate stabilizer-generator growth and no tight
state-of-the-art Schreier-Sims bound is claimed for this implementation.

No randomized Schreier-Sims, base/strong-generating-set optimization, permutation-
group intersection, coset enumeration, normal-series computation, solvability,
composition-series, cryptographic use, or benchmark claim is implied.
