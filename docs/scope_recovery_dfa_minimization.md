# Scope recovery — exact DFA minimization

## Coverage decision

A fresh live-state audit after the merged prime-field polynomial square-free
factorization checkpoint found no DFA minimization implementation, no matching
recovery branch, and no open implementation PR. Continuing finite-field
factorization immediately would have extended an already-adjacent polynomial
streak. This slice instead changes proof model to formal-language state
equivalence and partition refinement.

The historical `ROADMAP.md` remains presentation-drifted after the frozen
compiler/backend excursion, so this recovery slice does not invent a new phase
number or rewrite old roadmap history. Prospective authority remains the
post-Phase-69 scope-recovery decision plus live coverage audit.

## Contract

`minimize_dfa` accepts a complete deterministic automaton represented by:

- one explicit start state;
- a rectangular `state x alphabet` transition table;
- one accepting bit per state.

The alphabet may be empty. Malformed start/acceptance/transition dimensions or
out-of-range transition targets are rejected. Minimization is language-relative
to the start state: unreachable states are pruned and map to `std::nullopt`.

The returned quotient exposes both directions of the partition witness:

- `original_to_minimized` for every original state;
- `minimized_to_original` listing the sorted members of every reachable
  equivalence class.

Quotient states are renumbered canonically by BFS from the minimized start state,
following symbol indices in ascending order, so the minimized start state is
always zero and repeated execution is deterministic.

## Algorithm and invariant

Production uses first-principles Hopcroft partition refinement.

1. Reachability is computed from the start state and unreachable states are
   removed from the optimization domain.
2. The initial partition separates accepting and rejecting reachable states.
3. For a splitter block `A` and symbol `c`, predecessor lists construct
   `X = {q | delta(q,c) in A}`.
4. Every touched block `Y` is split into `Y intersect X` and `Y \\ X` when both
   are non-empty.
5. `block_of[q]` always identifies the current unique block containing reachable
   state `q`.
6. When a queued splitter is divided, both resulting parts remain represented in
   the worklist; otherwise only the smaller part is scheduled, which preserves
   the standard Hopcroft refinement bound.

At the fixed point, two reachable states share a block exactly when no suffix
word distinguishes their acceptance behavior. Quotient transitions therefore do
not depend on which block member is chosen as representative.

For `N` reachable states and alphabet size `K`, predecessor construction costs
`O(KN)` space/time and Hopcroft refinement has the standard `O(KN log N)` bound.
Canonical quotient BFS and witness materialization are linear in quotient
transitions plus partition membership.

## Independent verification

The focused candidate passed strict GCC, strict Clang, and actual ASan+UBSan
builds, each with five named tests.

Verification deliberately does not reuse Hopcroft refinement as its oracle:

- an independent classical table-filling distinguishability procedure computes
  state-pair equivalence on every randomized DFA;
- 800 fixed-seed random automata use 1–8 states and alphabet size 0–3;
- every reachable original-state pair is checked for exact agreement between
  table-filling equivalence and the returned quotient mapping;
- 80 additional random words per automaton compare original and minimized
  acceptance from their start states;
- deterministic cases cover malformed automata, an empty alphabet, unreachable
  pruning, equivalent-state merging, repeatable canonical numbering, a 64-state
  unary refinement chain whose states remain pairwise distinct, and a 96-state
  all-rejecting automaton that collapses to one state.

The tests are executable evidence for this implementation; the minimal-DFA
correctness theorem comes from the partition-refinement invariant rather than
from random sampling.

## Scope boundary

This slice minimizes already-constructed complete DFAs. It does not add regex
parsing, NFA determinization, epsilon closure, symbolic alphabets, weighted
transducers, lexer generation, or compiler/backend work. Those are separate
architectural questions and are not implied by this recovery checkpoint.
