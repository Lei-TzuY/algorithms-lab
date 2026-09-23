# Scope recovery: exact integer-weight alias sampler

## Coverage decision

After the ordered rooted-tree edit-distance checkpoint had already merged, a
fresh live-state audit found the default branch at
`e823e8f0ab23775d3ba628d603008e83915760c3`, with zero open pull requests
and zero open issues.

The same audit rejected several superficially attractive frontiers:

- rANS byte coding already had an occupied recovery branch;
- symmetric Lanczos tridiagonalization already had a recovery branch and PR;
- BCH coding already had an occupied recovery branch;
- stable roommates, maximum closure, path cover, KLL, and matroid union were
  already implemented or occupied;
- Monge recognition was adjacent to the sealed SMAWK totally-monotone surface.

No implementation, branch, or prior pull request was found for Walker/Vose
alias sampling.

This slice deliberately changes proof model again. It adds an exact finite
probability table whose correctness can be checked by counting every replay
state, rather than extending the recent graph, string, tree, hash-trie, or
numeric-linear-algebra families.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; historical compiler/backend work stays
frozen.

## Public contract

`ExactAliasSampler` accepts a nonempty vector of non-negative
`uint64_t` weights.

- zero-weight outcomes are permitted;
- at least one weight must be positive;
- the exact sum of all weights must fit in `uint64_t`;
- total-weight overflow is rejected;
- outcome indices are the original vector indices;
- the input weights remain queryable exactly.

For `n` outcomes with total weight `W`, the sampler represents the exact
categorical distribution

`P(i) = weight[i] / W`.

No floating-point normalization is used.

## Replay table semantics

Every outcome index is also one alias-table column.

Each column stores:

- `threshold` in `[0,W]`;
- one alias outcome index.

A deterministic replay call receives:

- a uniform column coordinate in `[0,n)`;
- a uniform threshold coordinate in `[0,W)`.

It returns the column itself when

`threshold_draw < threshold[column]`

and otherwise returns that column's alias.

Thus the complete replay universe has exactly `n * W` equally likely states.

The replay API is intentional, not a test-only hook: it separates deterministic
distribution decoding from the source of randomness and makes random executions
replayable from their bounded coordinates.

## Integer Vose construction

Let

`scaled[i] = n * weight[i]`.

Production evaluates this product in `unsigned __int128`, which is already an
accepted exact-arithmetic toolchain assumption in this repository's GCC/Clang
scope-recovery code.

Each scaled mass is classified against `W`:

- small: `scaled[i] < W`;
- large: `scaled[i] >= W`.

Repeatedly choose one small column `s` and one large column `l`.

The small column becomes final:

- primary mass: `scaled[s]`;
- alias: `l`.

The large residual becomes

`scaled[l] := scaled[l] + scaled[s] - W`.

That update preserves total residual mass while consuming exactly one full
column of capacity `W`.

The updated large entry is reclassified and the process repeats.

With exact integer arithmetic, when one work list becomes empty, every remaining
entry must have residual mass exactly `W`. Production checks this invariant
rather than silently rounding leftovers. Each remaining column is finalized as
a full self-column.

## Exact mass theorem

For any final cell `c`:

- `threshold[c]` replay states select `c` directly;
- `W - threshold[c]` replay states select `alias[c]`.

To verify the complete table, reconstruct for every outcome `i` the exact
number of replay states that select it.

Correctness requires

`mass[i] = n * weight[i]`.

Because the full replay universe contains `nW` states, dividing by `nW`
gives exactly

`P(i) = weight[i] / W`.

Production exposes `valid_distribution()`, which performs this exact
`unsigned __int128` reconstruction. The constructor also requires that
diagnostic to pass after construction.

Allocation failures from this diagnostic are not swallowed or converted into a
fake correctness failure: normal allocation exceptions propagate.

## Unbiased bounded draws

`sample(std::mt19937_64&)` needs two independent bounded uniforms:

1. a column in `[0,n)`;
2. a threshold in `[0,W)`.

Direct `random() % bound` would be biased whenever `bound` does not divide
`2^64`.

Production instead computes

`rejection = (-bound) mod bound`

in unsigned 64-bit arithmetic and rejects raw engine outputs below that value.

The accepted interval has size

`2^64 - rejection`,

which is an exact multiple of `bound`. Therefore reducing an accepted raw draw
modulo `bound` gives each bounded value exactly the same number of preimages.

This is exact discrete sampling under the contract that
`std::mt19937_64` supplies its standard uniform 64-bit output distribution.

The sampler makes no cryptographic-randomness claim.

## Independent verification

Committed tests verify the table from outside the builder in two independent
ways.

### Direct mass reconstruction

Tests read every public cell and independently accumulate:

- primary mass into the column outcome;
- residual mass into the alias outcome.

They then compare every reconstructed mass against `n * weight[i]`.

### Exhaustive replay-state enumeration

For small totals, tests enumerate every pair

`(column, threshold)`

with

- `column in [0,n)`;
- `threshold in [0,W)`.

Every state is passed through `sample_from_draws()`, and outcome counts are
compared exactly against `n * weight[i]`.

This verifies the deterministic decoding surface without statistical
tolerances.

Committed coverage includes:

- empty input rejection;
- all-zero rejection;
- exact total overflow rejection;
- a single outcome with `UINT64_MAX` weight;
- unequal weights `{1,2,3}`;
- zero-weight outcomes;
- equal weights;
- replay coordinate bounds;
- 500 fixed-seed random small weight tables with exact mass reconstruction and
  exhaustive state enumeration when the total is small;
- deterministic `mt19937_64` replay across two identically seeded engines;
- proof that the runtime sampler never returns a zero-weight outcome in that
  deterministic trace.

A supplementary independent model checked 490,000 random weight vectors and
verified both exact Vose residual closure and reconstructed replay mass with zero
mismatch. That is supporting evidence only; repository CI remains the
integration gate.

## Complexity boundary

For `n` outcomes:

- construction is `O(n)` time;
- resident storage is `O(n)`;
- `sample_from_draws()` is `O(1)`;
- one runtime sample performs two bounded rejection draws plus one constant-time
  table lookup;
- `valid_distribution()` is `O(n)` time and `O(n)` temporary exact-mass
  storage.

The rejection sampler's accepted interval is always nonempty; this slice states
exact unbiasedness, not a benchmark-backed latency claim.

## Non-claims

This slice does not claim:

- floating-point probability input;
- weighted sampling without replacement;
- dynamic weight updates;
- cryptographic randomness;
- entropy-optimal coding;
- a serialization format;
- SIMD/cache-optimal alias layout;
- benchmark-backed throughput;
- statistical evidence as a substitute for exact mass verification.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/randomized/exact_alias_sampler.hpp`;
- `tests/test_exact_alias_sampler_cases.hpp`;
- `docs/scope_recovery_exact_alias_sampler.md`.

Pre-PR hardening includes:

- avoiding brace-comma test expressions that can break macro parsing;
- preserving allocation failures from exact distribution diagnostics instead of
  swallowing them.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `e823e8f0ab23775d3ba628d603008e83915760c3`.
