# Scope recovery: deterministic SLR(1) shift-reduce parser generation

## Coverage decision

A fresh post-Phase-69 live audit at `main@7f9b38930d0bab93e0ab0233df93d3b1f6af1d65`
found no SLR, LR(0), canonical LR, LALR, or shift-reduce parser-generator
production surface in default-branch code and no occupied `slr` / `shift-reduce`
branch. The repository already contains exact CYK recognition for CNF grammars
and exact Earley recognition for general byte CFGs. This recovery therefore
changes proof model rather than adding another chart parser: it constructs a
finite viable-prefix automaton, derives SLR lookahead from FIRST/FOLLOW fixed
points, exposes ACTION/GOTO tables, and executes deterministic shift/reduce
transitions only when the table is conflict-free.

Prospective work remains governed by `docs/scope_recovery_after_phase69.md`; the
historical compiler/backend ROADMAP tail is not resumed.

## Production contract

`algorithms::automata::build_slr_parse_table` reuses the sealed Earley general-CFG
representation and its deterministic grammar validation/canonical rule ordering,
but does not reuse Earley chart-recognition state.

- grammar nonterminals and terminals retain the existing arbitrary-byte semantics;
- an internal augmented start production is added without entering the public
  normalized rule list;
- FIRST and nullable sets are computed to a fixed point, followed by FOLLOW sets
  with the distinguished end-of-input lookahead `256`;
- canonical LR(0) item sets are closed under nonterminal prediction and connected
  by deterministic GOTO transitions;
- terminal transitions install shift actions, completed original items install
  reductions only on FOLLOW(lhs), and augmented completion installs accept on EOF;
- distinct actions in one `(state, lookahead)` cell are retained as explicit
  `SlrConflict` witnesses rather than silently resolved by precedence or source
  order;
- conflicted tables cannot be executed by `slr_parse`;
- a conflict-free parse returns acceptance, consumed-byte count, and an ordered
  shift/reduce trace whose reduce ids index the public normalized rule list;
- a missing ACTION is an ordinary rejection, while malformed/internal table
  invariants surface as exceptions.

This is an SLR(1) surface. It does not claim canonical LR(1), LALR(1), GLR,
precedence/associativity resolution, parse forests, ambiguity counting, semantic
actions, error recovery, incremental parsing, or parser-generator code emission.

## Invariants and proof obligations

### LR(0) closure / GOTO

Each state is a canonical sorted set of dotted productions. Closure adds every
production `B -> . gamma` whenever some resident item expects nonterminal `B`.
GOTO on grammar symbol `X` advances exactly the items whose dot precedes `X` and
then closes the resulting kernel. State interning by complete item-set equality
therefore gives the canonical LR(0) viable-prefix automaton.

### FIRST / FOLLOW

FIRST and nullable are the least fixed point of terminal-prefix and nullable-prefix
propagation. FOLLOW starts with EOF in FOLLOW(start); for every occurrence
`A -> alpha B beta`, it adds FIRST(beta), and when beta is nullable it also adds
FOLLOW(A). SLR reductions consequently use grammar-level FOLLOW lookahead rather
than LR(1) item-specific lookahead.

### Conflict boundary

A table is executable only when each ACTION cell has at most one distinct action.
Shift/reduce and reduce/reduce collisions are observable witnesses. The
implementation deliberately does not choose one action, so an ambiguous or
non-SLR grammar cannot accidentally acquire undocumented semantics.

### Runtime stack

The state stack starts at LR state zero. A shift consumes exactly one input byte
and pushes its target state. Reducing `A -> beta` pops exactly `|beta|` states,
then follows the GOTO for `A` from the newly exposed state. Thus every successful
trace corresponds to the normalized grammar's shift/reduce derivation. Accept is
legal only on EOF after augmented-start completion. A repeated `(input position,
state stack)` configuration is rejected as a non-progressing reduction cycle.

## Verification

Focused candidate bytes pass:

- GCC C++20 repository strict warnings-as-errors: 5/5;
- Clang C++20 repository strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with leak detection/fail-fast: 5/5.

Deterministic evidence covers:

- the classic left-recursive expression grammar with encoded precedence;
- epsilon productions and arbitrary `0x00` / `0xff` terminal bytes;
- ordinary malformed-input rejection;
- explicit shift/reduce and reduce/reduce conflicts with execution refusal;
- invalid grammar references and an empty-language grammar;
- repeat-build determinism;
- independent replay of every accepted shift/reduce trace as a grammar-symbol
  stack, checking every shifted byte and every reduced RHS exactly.

Primary randomized evidence generates 300 deterministic finite automata with one
to four states and a three-byte alphabet, converts each DFA to an equivalent
right-linear CFG, and tests 25 random byte strings of length zero through seven.
That yields 7,500 recognition cases whose expected answer comes directly from DFA
simulation rather than another parser. Every generated grammar must build a
conflict-free deterministic SLR table, and every accepted trace is replayed
independently. The sealed Earley recognizer is also required to agree, but it is
secondary cross-implementation evidence rather than the primary oracle.

Tests are implementation evidence; they do not replace the standard LR viable-
prefix and SLR FOLLOW-set correctness arguments.

## Complexity and non-claims

Let `Q` be the total number of dotted positions across the augmented grammar,
`S` the number of canonical LR(0) states actually materialized, and `A` the
number of automaton transitions. `S` can be exponential in grammar size; this
implementation makes no polynomial-in-grammar-size table-construction claim.
The direct educational construction scans item vectors, computes closure from
explicit production lists, and interns whole item sets in ordered maps, so its
cost is polynomial in the explicitly materialized `(S,Q,A)` automaton but is not
presented as an optimized parser-generator bound.

For an already-built conflict-free table, execution performs one table action per
shift/reduction plus diagnostic stack/configuration checks. It makes no blanket
linear-time claim for arbitrary hostile grammars because epsilon/unit structures
can create non-progressing reduction configurations, which are detected and
reported.

The slice also makes no ambiguity-decision claim for arbitrary CFGs: an SLR
conflict proves this generated table is not deterministic under the SLR policy;
it is not a general ambiguity theorem.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/automata/slr_parser.hpp`;
- `tests/test_slr_parser_cases.hpp`;
- `docs/scope_recovery_slr_parser.md`.

The sealed recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. No CMake, ROADMAP, README, workflow, benchmark, frozen
compiler/backend, or temporary-file churn is required.
