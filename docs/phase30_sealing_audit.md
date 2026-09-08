# Phase 30 Sealing Audit

## Gate

Phase 30 is sealed only after the exact seed-and-verify implementation merged and the merged `main` candidate was revalidated. The implementation PR #77 merged as `main@0eeeb0100608e1da7bd09c07840bcd91cd7fa021`. Push CI run `34191341084` completed successfully on all three repository gates: GCC release, Clang release, and GCC ASan+UBSan.

At seal time there are no open pull requests, no open issues, and no `phase-31*` branch competing for the same BWT extraction/search surface.

## Correctness audit

The implementation closes the promoted Phase-30 capability without weakening exactness:

- `k+1` non-overlapping exact seeds are a completeness argument, not a heuristic ranking rule;
- every exact seed occurrence expands to the complete clipped `q - b ± k` candidate-start band required by preceding insertion/deletion displacement;
- candidate starts are deduplicated before verification;
- no-candidate queries skip text reconstruction because the seed completeness argument itself proves absence within budget;
- non-empty candidate sets trigger one exact Phase-29 reconstruction and final membership is decided only by bounded exact Levenshtein dynamic programming;
- empty-pattern / `k >= |pattern|` behavior remains aligned with the sealed Phase-27 exact search contract.

Deterministic tests cover substitution, insertion, deletion, negative mathematical seed centers, repetitive candidate deduplication, no-reconstruction absence, arbitrary bytes, and locate sample-rate variation. Random evidence includes 1,200 fixed-seed cases against an independent direct substring-DP oracle and 180 bounded cases cross-checked against both that oracle and the sealed Phase-27 state-expansion solver.

The exact merged candidate also passed the full repository suite under both release compilers and ASan+UBSan.

## Claim audit

The phase does not claim a universal speedup over Phase 27. Candidate count may be `Theta(n)` on repetitive input, and each verified candidate has an explicit polynomial DP cost. The implementation exposes seed-hit, unique-candidate, verified-candidate, DP-cell, and reconstruction-LF-step diagnostics rather than substituting CI timing or an uncontrolled benchmark for complexity evidence.

No heuristic-seed, probabilistic, compressed-optimal extraction, r-index, or hidden polynomial-index claim is made.

## Architecture audit

Phase 30 is a coherent architecture comparison and should not be extended with another scoring or seed partition variant merely to increase algorithm count. Its remaining material cost comes from the sealed Phase-29 extraction substrate: once at least one candidate exists, the implementation reconstructs the entire source text even when verification only needs bounded candidate windows.

The repository already contains Phase-20 periodic suffix-position samples with a bounded LF locate distance. Those samples currently map conceptual rows to suffix positions, but there is no resident inverse from sampled suffix position back to row. Adding that inverse enables an exact local extraction algorithm: choose the smallest periodic sampled suffix position at or after the requested range end, then LF-walk backward only until the requested begin position while recording bytes that fall inside the range.

This is a substantial cross-phase integration frontier because it turns Phase-20 locate sampling into a random-access/extraction substrate and gives Phase 30 a future way to avoid unconditional full reconstruction. It is not a cosmetic optimization of the same search loop.

## Promotion

Phase 31 is promoted as **periodic-sample local BWT extraction**.

Acceptance criteria:

- keep the sealed row-to-position locate samples intact;
- add an explicit suffix-position-to-row inverse for those same periodic samples, including the final `n` sample when `n` is not a multiple of the sample rate;
- expose validated half-open local extraction over arbitrary bytes without consulting a retained source-text copy;
- start from the nearest sampled suffix position at or after `end` and LF-walk backward only as far as `begin`;
- expose LF-step and inverse-sample resident-payload diagnostics;
- verify exact slices against independent constructor-text substrings across empty/classic/arbitrary-byte and randomized inputs with varied sample rates;
- demonstrate the deterministic bound `lf_steps <= (end - begin) + locate_sample_rate - 1` for non-empty ranges;
- make the extra resident `O(n / sample_rate)` inverse-sample state explicit;
- make no compressed-optimal random-access or universal speedup claim.

Only after that substrate is merged and integrated should seed-and-verify itself be reconsidered for local-window verification.
