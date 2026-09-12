# Scope recovery: complete prime-field irreducible factorization

This checkpoint closes the previously explicit orchestration gap between the
sealed prime-field polynomial layers. It does not add another splitting
algorithm: it composes square-free factorization (SFF), distinct-degree
factorization (DDF), and equal-degree factorization (EDF) into one end-to-end
`F_p[x]` factorization contract.

## Public contract

`polynomial_irreducible_factorization_mod(f, p, seed, max_trials_per_group)`:

- accepts the same canonical prime-field polynomial representation as the
  sealed Euclid/SFF/DDF/EDF layers;
- rejects the zero polynomial, composite moduli, and non-canonical
  coefficients through the sealed validation path;
- preserves the original nonzero leading unit;
- returns sorted monic irreducible factors paired with exact positive
  multiplicities;
- exposes the caller seed and total EDF trials executed;
- uses a fixed SplitMix64 mapping to derive one replayable EDF seed per
  canonical `(multiplicity, irreducible-degree)` traversal group;
- applies `max_trials_per_group` independently to every EDF group;
- if any randomized group exhausts its finite budget, returns
  `complete=false` with no partial factor list.

Constants complete with no factors and zero trials. Randomness affects only
whether a nontrivial EDF split is found within the finite budget; every factor
accepted by EDF is exact.

## Correctness composition

Unique factorization over `F_p[x]` is the proof boundary.

1. SFF writes the monic input as pairwise-coprime square-free layers and
   associates each layer with its exact multiplicity.
2. DDF partitions each square-free layer into products of irreducibles of one
   degree.
3. EDF splits each such product into individual monic irreducibles, accepting
   only exact GCD/quotient splits.
4. The orchestrator attaches the SFF multiplicity to every resulting
   irreducible and restores the original unit.

Therefore a complete result reconstructs the input exactly and contains only
monic irreducibles with their unique multiplicities. Finite tests are
implementation evidence; they are not a proof of the finite-field
factorization theorems.

## Verification

The dedicated test layer uses the already-independent small-field oracle from
the DDF/EDF tests: monic candidates are enumerated and irreducibility is
classified by naive lower-degree divisibility rather than Frobenius/GCD/EDF
splitting.

Evidence covers:

- zero/composite/non-canonical validation and constant semantics;
- explicit EDF budget exhaustion with fail-closed no-partial-output behavior;
- characteristic-two repeated factors requiring p-th-root SFF recursion;
- replay equality under a fixed seed;
- a full-width `uint64_t` prime with repeated linear factors and a nontrivial
  leading unit;
- 120 fixed-seed random factorizations over `F_2`, `F_3`, and `F_5`, mixing
  independently classified irreducibles of degrees one through three with
  multiplicities one through three;
- independent exact reconstruction of every complete output.

## Complexity and non-claims

For input degree `n`, this direct orchestration inherits the costs of the
sealed SFF/DDF layers plus the EDF work actually executed. With `T` total EDF
trials, a conservative educational bound is
`O((T*n + n) * n^3 * log^2(p))` time and `O(n^2)` transient/result polynomial
storage; this intentionally favors an honest upper bound over a tighter claim
for a different implementation.

No deterministic finite-budget completion, Berlekamp nullspace method,
extension-field factorization, fast/subquadratic polynomial arithmetic,
arbitrary-precision coefficient domain, or cryptographic randomness claim is
made.
