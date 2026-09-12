# Scope recovery: Earley general-CFG recognition

## Coverage decision

A fresh live audit at `main@3f4df2c10ad028ba94dd643759ed7279fc6d8a86`
found the sealed CNF-only CYK parser but no Earley recognizer, general-CFG parser,
Earley branch, or historical Earley implementation. The CYK recovery document
explicitly leaves general CFG normalization, arbitrary nullable/unit productions,
and Earley parsing outside its claim. This slice changes proof model after the
recent Kirchhoff / finite-field factorization streak instead of farming another
graph or polynomial variant.

`docs/scope_recovery_after_phase69.md` remains the prospective governance
authority; the frozen compiler/backend ROADMAP surface is untouched.

## Production contract

`earley_recognize(grammar, input)` accepts an explicit byte-oriented CFG with:

- arbitrary-length right-hand sides;
- epsilon productions;
- unit productions and cyclic unit dependencies;
- direct and indirect left recursion;
- arbitrary byte terminals including `0x00` and `0xff`;
- duplicate productions, canonicalized before recognition;
- deterministic chart-item-count diagnostics for replay.

The API performs exact recognition only. It deliberately does not claim parse
forest construction, ambiguity counting, error recovery, incremental parsing,
CFG normalization, or parser-generator functionality.

## Earley fixed-point obligation

An item `(A -> alpha . beta, origin)` in chart column `i` means that `alpha`
derives exactly the consumed slice from `origin` to `i`.

- prediction adds every rule of the next expected nonterminal at the current
  input position;
- scanning advances a terminal item exactly when the next input byte matches;
- completion advances every item in the origin column waiting for the completed
  nonterminal;
- nullable catch-up also advances a waiter when its zero-width completion was
  already processed earlier in the same chart column.

The last rule is important for epsilon/unit/left-recursive grammars: insertion
order must not change the least fixed point. Deduplication bounds the finite item
universe, so agenda processing terminates. A word is accepted exactly when a
start-symbol rule originating at zero is complete in the final chart column.

Correctness rests on the classical Earley item invariant / least-fixed-point
argument; finite tests are implementation evidence rather than a theorem proof.

## Independent verification

The primary oracle is not another Earley parser. For bounded grammars it computes
the least fixed point of span facts `derives(A, begin, end)`. Each production is
replayed left-to-right over currently known terminal matches and nonterminal span
facts, and the process iterates until no new span can be added. This handles
nullable rules and recursive cycles through a structurally different bottom-up
closure.

Evidence in the candidate includes:

- ordinary multi-symbol RHS acceptance/rejection;
- epsilon chains, cyclic unit productions, and direct left recursion;
- a dedicated late-nullable-waiter regression that fails without same-column
  nullable catch-up;
- duplicate-rule canonicalization and deterministic chart replay;
- arbitrary NUL/high-byte terminals;
- malformed start/lhs/symbol validation;
- 1,000 fixed-seed random general CFGs with 1--4 nonterminals, arbitrary RHS
  lengths 0--3, and input lengths 0--5, compared exactly to the fixed-point
  span oracle;
- 500 additional random CNF grammars cross-checked against the sealed CYK parser
  on their common semantics. CYK is secondary integration evidence, not the
  primary oracle.

Focused repo-native GCC strict-warning, Clang strict-warning, and actual
ASan+UBSan execution all pass 5/5 on the final candidate shape before upload.

## Complexity and non-claims

Let `Q` be the number of dotted production positions after canonicalization.
The direct chart stores `O(n^2 Q)` possible items. Because this educational
baseline explicitly scans origin/current columns for completion and nullable
catch-up, it states the conservative bound `O(n^3 Q^2)` time and `O(n^2 Q)`
item/storage state. For a fixed grammar this is the classical cubic worst-case
shape.

No unambiguous-grammar quadratic optimization, Leo optimization, parse-forest
compression, ambiguity count, incremental update, streaming parser, error
recovery, GLR/LL/LR equivalence, or sub-cubic general-CFG claim is made.

## Scope

Exactly seven files differ from main:

- `include/algorithms/automata/earley_parser.hpp` (public header);
- `include/algorithms/automata/detail/earley_parser_grammar.hpp` (normalization/item invariants);
- `include/algorithms/automata/detail/earley_parser_engine.hpp` (header-only chart engine);
- `tests/test_earley_parser_cases.hpp` (deterministic / validation evidence);
- `tests/test_earley_parser_randomized_cases.hpp` (differential / CYK integration evidence);
- two include lines in `tests/test_main.cpp`;
- this proof / boundary document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
compiler/backend, or temporary-file churn is required.
