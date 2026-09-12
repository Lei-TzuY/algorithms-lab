# Scope recovery: epsilon-NFA determinization

## Coverage decision

A fresh live-state audit after the static 2D orthogonal range-tree checkpoint found no epsilon-NFA representation, epsilon closure, subset construction, NFA determinization, equivalent pull request, or overlapping recovery branch. The existing Hopcroft DFA minimizer explicitly left this surface out of scope. This slice therefore fills a distinct formal-language gap instead of extending the recent spatial-index streak or the frozen compiler/backend history.

`docs/scope_recovery_after_phase69.md` remains the prospective authority. Historical `ROADMAP.md` is intentionally untouched.

## Production contract

`EpsilonNfa` represents:

- one explicit start state;
- an accepting-state vector;
- zero or more epsilon-transition targets per state;
- a rectangular `state x symbol` table whose cells contain zero or more target states.

Duplicate targets are legal and semantically idempotent. An empty alphabet is legal. State IDs and every transition endpoint are validated.

`determinize_epsilon_nfa(nfa, max_dfa_states)` returns:

- a complete `Dfa` compatible with the sealed Hopcroft minimizer;
- a sorted NFA-state subset witness for every DFA state;
- canonical DFA numbering by BFS from the start epsilon closure, visiting symbols in ascending order;
- the empty subset as an ordinary rejecting dead state when reachable.

The explicit positive `max_dfa_states` budget acknowledges the classical exponential subset-state explosion. The routine throws `std::length_error` before adding a state that would exceed the budget.

## Correctness obligation

For a subset `S`, let `closure(S)` be exactly the NFA states reachable from `S` through zero or more epsilon transitions. The initial DFA state represents `closure({start})`.

For each DFA subset `S` and symbol `a`, the transition target is

`closure({v | exists u in S with u -a-> v})`.

A DFA subset is accepting iff it contains at least one accepting NFA state. By induction on consumed-word length, the DFA subset after a word is exactly the epsilon-closed active-state set of the NFA after that word; therefore acceptance is language-equivalent. BFS discovery order plus sorted subset witnesses makes the representation deterministic without changing language semantics.

The implementation deliberately does not claim polynomial worst-case determinization. If `R` subset states are reachable, the direct educational implementation performs an epsilon closure for every `(subset,symbol)` edge and uses ordered-map lookup on sorted subset vectors. A conservative bound is exponential through `R <= 2^N`; no stronger optimized subset-construction bound is claimed.

## Independent verification

The primary oracle does not call subset construction. It directly simulates the NFA by repeatedly computing epsilon closure and symbol moves.

Focused pre-upload evidence passed under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

Evidence includes:

- malformed start/vector/row/endpoint validation;
- explicit zero-state-budget rejection and bounded state-explosion failure;
- epsilon cycles, duplicate transitions, empty alphabet and epsilon-only acceptance;
- reachable empty dead subset and deterministic replay;
- 500 fixed-seed random NFAs with 1-6 states and alphabet size 0-3;
- exhaustive comparison of every word of length at most five against the direct epsilon-closure simulator;
- replay of every returned subset witness and every subset transition.

As secondary cross-layer evidence, the determinized DFA is passed through the already-sealed Hopcroft minimizer and the minimized automaton is checked against the same direct NFA simulator. Hopcroft is not the primary oracle for determinization correctness.

## Scope / non-claims

This slice adds no regex parser, Thompson regex compiler, lexer generator, symbolic alphabet, NFA minimizer, weighted automaton, transducer, parser-generator surface, compiler/backend functionality, benchmark, or claim that finite tests prove the subset-construction theorem.

The recovery diff is intentionally narrow: header-only production, appended tests in the already-registered DFA test translation unit, and this proof document. No CMake, README, ROADMAP, workflow, benchmark, or recovery-authority churn is required.
