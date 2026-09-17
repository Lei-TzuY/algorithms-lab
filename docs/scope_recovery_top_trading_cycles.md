# Scope recovery: Shapley-Scarf top trading cycles

## Coverage decision

A fresh live audit at `main@c8e95a215df5ea7411214c3227d088be7cf460ef`
found no top-trading-cycles / Shapley-Scarf housing-market implementation in
default-branch code, pull-request history, or branch names. The repository does
already contain bipartite stable matching and exact Stable Roommates, but those
solve preference matching between people; this slice instead allocates initially
endowed indivisible houses and introduces the housing-market core theorem.

The prospective authority remains `docs/scope_recovery_after_phase69.md`. The
historical ROADMAP and frozen Phase-45--69 compiler/backend sequence are not
modified.

## Production contract

`top_trading_cycles(preferences)` models `n` agents and `n` houses, with house
`i` initially owned by agent `i`.

- every preference row must be a complete strict permutation of all houses;
- empty input returns the empty allocation;
- each round every active agent points to their most-preferred active house and
  each active house points to its original owner;
- every directed cycle trades simultaneously and all agents/houses in those
  cycles leave the market;
- output contains the final house permutation plus deterministic round/cycle
  witnesses;
- cycles are rotated to their smallest agent id and cycle lists are sorted by
  that first id, so repeated execution is byte-for-byte deterministic.

The implementation uses monotonically advancing preference cursors. Because an
active agent's own house remains active until that agent leaves, every active
agent always has a valid current choice.

## Core / correctness boundary

For complete strict preferences in the Shapley-Scarf housing market, the
classical top-trading-cycles theorem says that repeated simultaneous cycle
trading terminates at the unique core allocation.

A blocking coalition can cyclically reassign only houses initially owned by its
members. For a candidate allocation create a directed arc `i -> j` whenever
agent `i` weakly prefers house `j` (initially owned by `j`) to their assigned
house, and mark the arc strict when the preference is strict. A blocking
coalition exists exactly when there is a directed weak-preference cycle
containing at least one strict arc; equivalently, some strict arc `u -> v` has a
weak path from `v` back to `u`. A core allocation has no such cycle.

The TTC/core theorem is a mathematical proof obligation. Finite tests validate
this implementation and returned witnesses; they are not presented as a proof
of the theorem.

## Verification

Focused exact candidate verification passes under:

- GCC C++20 repository strict warnings-as-errors;
- Clang C++20 repository strict warnings-as-errors;
- actual GCC ASan+UBSan with fail-fast behavior.

Deterministic evidence covers malformed rows, duplicate/out-of-range houses,
empty input, one simultaneous three-agent trading cycle, a two-round market with
a first-round self-cycle, all-self cycles, round witness replay, and repeated
execution equality.

The primary oracle is independent from TTC. It enumerates every house allocation
(permutation), builds the weak-preference owner graph described above, rejects
allocations whenever a weak directed cycle contains at least one strict arc, and
therefore enumerates the core directly. Tests exhaustively cover every strict
preference profile through three agents and require exactly one core allocation
equal to production. A further 360 fixed-seed random profiles with one through
seven agents repeat the same exact allocation enumeration/core check.

The witness replay independently recomputes each active agent's favorite
remaining house round by round and checks every returned cycle edge and final
assignment.

## Complexity / non-claims

Each agent's preference cursor advances at most `n` positions across the entire
run. Functional-graph cycle discovery costs `O(n)` per round and there are at
most `n` rounds, so this direct implementation is `O(n^2)` time with `O(n)`
mutable working state beyond the input and returned witness.

No ties, incomplete preferences, multiple ownership/endowments, quotas,
randomized housing allocation, strategyproofness implementation experiment,
competitive-equilibrium prices, top-trading-cycles-and-chains kidney exchange,
or general matching/core solver is claimed.
