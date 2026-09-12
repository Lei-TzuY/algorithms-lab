# Scope recovery: exact parity-game winning regions

## Coverage decision

Fresh live-state, pull-request-history, default-branch code, and branch searches at
`main@b4783fc3289bb72036b8315c44da47c84e03c241` found no parity-game,
Zielonka, reachability-game, or retrograde-game solver. The latest merged recovery
slice is cactus-forest block decomposition; the long-lived minimum-cycle-basis
surface remains occupied and is intentionally untouched. This slice deliberately
changes proof model from graph decomposition to infinite-duration game solving.

## Production contract

`solve_parity_game(graph, owner, priority)` solves a finite total directed parity
game under the **maximum recurring priority** convention:

- every vertex is owned by `even` or `odd`;
- every vertex must have at least one outgoing move, so every play is infinite;
- the winner of an infinite play is the parity of the maximum priority that occurs
  infinitely often;
- directed inputs are required; self-loops and parallel arc copies are allowed;
- edge weights are intentionally ignored;
- empty games return an empty winning partition;
- owner/priority shape and owner-domain violations fail closed.

The result reports the exact winner of every vertex plus replayable implementation
diagnostics (`recursive_calls` and attractor insertions). It deliberately does not
claim to expose a canonical positional strategy.

## Algorithm / proof boundary

Production uses Zielonka's recursive decomposition. In each active subgame it:

1. takes the highest active priority `p` and its player `alpha`;
2. computes `alpha`'s attractor to all priority-`p` vertices;
3. solves the remaining subgame recursively;
4. if the opponent has no winning vertex there, the entire attractor belongs to
   `alpha`;
5. otherwise it computes the opponent attractor to that opponent-winning region
   in the original subgame, removes it, and recurses again.

Attractors are computed from a reverse edge-copy index with remaining active
outdegree counts, so parallel arcs and self-loops follow the repository multigraph
semantics without changing the game-theoretic relation.

Correctness relies on positional determinacy of finite parity games and the
classical Zielonka decomposition theorem. Finite tests are implementation
evidence, not proofs of those theorems.

## Independent verification

Focused exact-candidate verification passed:

- GCC C++20 strict warnings-as-errors: 5/5;
- Clang C++20 strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan: 5/5.

Deterministic cases cover empty/shape/directed/total-game validation, invalid enum
owners, even/odd self-loop games, owner-controlled attractor choices, parallel
copies with ignored signed weights, disconnected components, and repeated
execution.

The primary randomized oracle is structurally independent from attractors and
Zielonka recursion. For 180 fixed-seed total directed multigraphs with 1..6
vertices and at most two distinct choices per vertex, tests enumerate **every
memoryless strategy for both players**. For every start vertex they evaluate every
strategy pair by following the induced deterministic play to its eventual cycle,
then classify that cycle by the maximum recurring priority. The test accepts a
winner only when one player has a strategy that defeats every opposing positional
strategy; the two quantified results must be complementary. Production must match
that exact per-vertex oracle vector.

The positional-strategy sufficiency used by this oracle is the same classical
parity-game theorem boundary; the oracle is independent at the algorithmic level
because it contains no attractor or Zielonka recurrence.

## Complexity / non-claims

One attractor computation is `O(V+E)` on an active subgame. This direct recursive
Zielonka implementation can make exponentially many recursive calls in the worst
case; a conservative bound is `O(2^V (V+E))` time. Because active/winning masks are
copied across recursion levels, the direct implementation uses `O(V^2 + E)`
working storage including recursion-state vectors, plus `O(V)` process recursion
depth.

No small-progress-measures bound, strategy-improvement bound, quasi-polynomial
parity solver, canonical winning strategy, stochastic game, mean-payoff game, or
benchmark-backed performance claim is implied.

## Scope

Exactly four paths differ from the live base:

- `include/algorithms/games/parity_game.hpp`;
- `tests/test_parity_game_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_parity_game.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
minimum-cycle-basis, frozen compiler/backend, or temporary-file surface changes.
