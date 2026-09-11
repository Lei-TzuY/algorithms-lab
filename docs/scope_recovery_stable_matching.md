# Scope recovery: proposer-optimal stable matching

## Capability

This recovery slice adds one-to-one stable matching for two equally sized sides
with complete strict preferences. The production implementation is the
proposer-side Gale-Shapley deferred-acceptance algorithm. It returns both inverse
matching maps plus an explicit proposal count.

The contract is intentionally narrow: preference rows must each be a permutation
of `[0,n)`. Ties, incomplete lists, unequal sides, many-to-one hospitals/residents,
stable roommates, weighted objectives, and rotation-poset enumeration are not
claimed.

## Invariants and proof obligations

Each proposer owns a monotone `next_choice` index and therefore proposes to any
receiver at most once. A receiver is either unmatched or holds the most-preferred
proposer it has seen so far; receiver partners can only improve according to that
receiver's strict ranking. Consequently at most `n^2` proposals occur.

When the algorithm ends, suppose proposer `p` and receiver `r` formed a blocking
pair. Because `p` prefers `r` to its final partner, `p` must have proposed to `r`
earlier. Receiver `r` either rejected `p` immediately or later displaced `p` for
a proposer it preferred; since receiver partners only improve, `r` cannot prefer
`p` to its final partner. This contradiction proves stability.

Proposer optimality uses the standard rejection lemma: when a receiver rejects a
proposer, that pair cannot belong to a stable matching in which the proposer is
better off than in proposer-side deferred acceptance. Repeated rejection therefore
shows every proposer receives its best partner among all stable matchings. This
is a theorem obligation; randomized testing is implementation evidence rather
than a substitute proof.

## Complexity

Receiver inverse rankings require `O(n^2)` preprocessing/storage. Every ordered
proposer/receiver pair is proposed at most once, so deferred acceptance is
`O(n^2)` time and `O(n^2)` total auxiliary/result storage including the input-rank
matrix.

## Verification

Deterministic tests cover empty/singleton instances, malformed matrices,
duplicate/out-of-range preference entries, a displacement chain, replay
determinism, and a shared-first-choice stress case.

The independent randomized oracle enumerates every perfect matching for 450
fixed-seed instances with up to six participants per side, filters stable
matchings by a direct blocking-pair scan, then verifies the production matching
is stable and gives every proposer the minimum preference rank attained in any
stable matching. The oracle does not run Gale-Shapley.

## Recovery scope

This slice follows the post-Phase-69 scope-recovery rule: it adds a proof model
absent from live code and changes paradigm after maximum clique and matroid
intersection. It does not resume the frozen compiler/backend surface and does not
modify the historically stale ROADMAP presentation.
