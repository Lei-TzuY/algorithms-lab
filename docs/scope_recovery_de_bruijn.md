# Scope recovery: deterministic FKM de Bruijn sequences

## Coverage decision

After exact byte-regex derivatives merged as
`main@2c408ef23b6aa912b23ec24252f451831f3350ca`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains:

- exact Eulerian trails/circuits;
- linear-time Lyndon factorization;
- Burnside orbit counting.

However, it had no de Bruijn-sequence constructor, occupied de Bruijn branch,
or prior implementation pull request.

This slice deliberately does not wrap the Eulerian-trail implementation and
does not call the existing Lyndon-factorization implementation. It uses the
Fredricksen-Kessler-Maiorana necklace recursion directly, giving a distinct
construction proof surface.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; frozen compiler/backend history is
untouched.

## Public contract

`de_bruijn_sequence(k,n)` constructs one deterministic cyclic de Bruijn
sequence `B(k,n)`.

Parameters:

- `k = alphabet_size >= 1`;
- `n = order >= 1`;
- symbols are integer values `0..k-1`.

The returned vector is the cyclic representation itself:

- its length is exactly `k^n`;
- it does **not** append the first `n-1` symbols again;
- every cyclic window of length `n` is a distinct word over the alphabet;
- therefore all `k^n` length-`n` words occur exactly once.

Invalid zero parameters are rejected.

If `k^n` cannot fit in `size_t` or vector capacity, construction rejects
the request rather than wrapping arithmetic.

For `k=1`, every order has the unique one-symbol cyclic representation
`[0]`, so production returns it directly without allocating an
order-proportional workspace.

## FKM recurrence

Production maintains a 1-based workspace `a[1..n]` plus sentinel `a[0]=0`.

The recursive state is `db(t,p)`, where:

- `t` is the next position to assign;
- `p` is the current candidate period.

For `t <= n`:

1. copy the period continuation: `a[t]=a[t-p]`, then recurse with `(t+1,p)`;
2. for each larger alphabet symbol `j>a[t-p]`, set `a[t]=j` and recurse
   with new period `t`.

For `t>n`:

- if `p` divides `n`, append `a[1..p]`;
- otherwise append nothing.

Concatenating these selected necklace representatives is the FKM de Bruijn
construction.

Production verifies that the number of emitted symbols is exactly the checked
value `k^n`; any internal violation raises `std::logic_error`.

## Why no explicit de Bruijn graph is built

A common alternative constructs the order-`n` sequence as an Eulerian cycle
in the de Bruijn graph whose vertices are length-`n-1` words.

That route is already adjacent to the repository's Eulerian-trail machinery and
would allocate graph structure proportional to the word space.

This slice instead exercises the necklace-period recurrence directly. The
resulting executable capability is the same mathematical object, but the
construction invariant is different.

## Independent verification

Committed tests do not reuse the FKM recurrence.

For a returned sequence of length `L=k^n`, the oracle examines every cyclic
start position `s=0..L-1`.

It reads the next `n` symbols cyclically and base-`k` encodes that word to an
integer in `0..k^n-1`.

Exact validity requires:

- every symbol is less than `k`;
- every encoded word index is in range;
- no encoded word is seen twice;
- after all `L` starts, every one of the `k^n` word codes has been seen.

This is definition-level verification and is independent of necklace periods,
Lyndon factorization, recursion state, or Eulerian graph construction.

Committed coverage includes:

- zero-alphabet and zero-order rejection;
- arithmetic overflow rejection;
- the unique one-symbol alphabet for orders including a large order;
- the deterministic binary order-three FKM output
  `00010111`;
- every parameter pair `1 <= k <= 5`, `1 <= n <= 5`;
- nonbinary medium cases `B(3,6)`, `B(4,5)`, and `B(7,4)`;
- repeat-call determinism.

A supplementary model-level definition check examined 82,200 cyclic windows
across an additional small parameter grid and found zero duplicate or missing
word. This is supporting evidence only; repository CI remains the integration
gate.

## Complexity boundary

Let `L=k^n`, which is also the unavoidable output size.

The FKM construction emits exactly `L` symbols.

The standard CAT property of the FKM necklace generation gives output-linear
generation behavior; this slice conservatively relies only on the practical
fact that every emitted symbol is part of the required `Theta(L)` output.

Resident storage is:

- `Theta(L)` for the returned sequence;
- `O(n)` recursion/workspace state.

The implementation does not build the `k^(n-1)`-vertex de Bruijn graph.

## Non-claims

This slice does not claim:

- lexicographically minimal rotation as part of the public contract;
- universal-cycle support for arbitrary combinatorial objects;
- random de Bruijn sampling;
- streaming output without retaining the returned vector;
- Eulerian-graph construction;
- reuse of the repository's Lyndon-factorization algorithm;
- benchmark-backed speed or memory superiority.

The exact de Bruijn coverage property is the claim being tested.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/combinatorics/de_bruijn.hpp`;
- `tests/test_de_bruijn_cases.hpp`;
- `docs/scope_recovery_de_bruijn.md`.

Pre-PR hardening added the explicit `<utility>` include required by the test's
pair/structured-binding cases.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `2c408ef23b6aa912b23ec24252f451831f3350ca`.
