# Scope recovery: exact CYK parsing for byte grammars

## Coverage decision

After static arithmetic byte coding reached merged `main`, a fresh live-code,
pull-request-history, and branch audit found no context-free grammar, CYK,
Cocke-Younger-Kasami, or equivalent chart-parsing capability in
`algorithms-lab`. The immediately adjacent LZ77/LZW/range-coder surfaces were
not selected because they would extend the recent coding streak without changing
the proof model. Metric-TSP approximation was also considered, but the direct
MST-preorder baseline would add less structural depth than an exact grammar
recognizer with a reconstructable derivation witness.

This slice follows `docs/scope_recovery_after_phase69.md`: the historical
Phase-45-69 compiler/backend surface remains frozen, and prospective work is
selected by fresh algorithm/data-structure coverage rather than by the stale
historical ROADMAP frontier.

## Public contract

`cyk_parse(grammar, input)` recognizes an arbitrary byte string under an explicit
Chomsky-normal-form grammar.

- A grammar has `N > 0` numbered nonterminals, a valid start nonterminal,
  terminal productions `A -> byte`, and binary productions `A -> B C`.
- The only epsilon capability is the explicit `start_accepts_empty` flag. Unit
  productions and general CFG normalization are intentionally outside scope.
- Terminal bytes use unsigned-byte semantics, including `0x00`, `0x80`, and
  `0xff`.
- Duplicate productions are accepted and canonicalized.
- Rejection returns `accepted=false` with no witness nodes/root.
- Empty input is accepted exactly when `start_accepts_empty` is true; its witness
  is intentionally empty.
- A non-empty accepted input returns a flat postorder parse tree. Every child
  index is smaller than its parent index and the root spans the complete input.

Ambiguous inputs use a deterministic witness policy. Spans examine split points
from left to right. Binary productions are canonicalized lexicographically by
`(lhs,left,right)`, and the first derivation discovered for a chart state is
retained. Terminal productions are likewise canonicalized.

## Chart invariant and correctness obligation

For input length `n`, the dense chart stores a boolean state

`reachable(begin, length, A)`

for every valid non-empty span and nonterminal `A`. Its intended invariant is:

> `reachable(i,l,A)` is true exactly when `A` derives the byte substring
> `input[i:i+l]` under the supplied CNF grammar.

Length-one initialization is exact because a state is marked iff a terminal rule
`A -> input[i]` exists. For a longer span, CYK enumerates every split
`1 <= k < l` and every binary rule `A -> B C`. It marks `A` iff the left chart
contains `B` on length `k` and the right chart contains `C` on length `l-k`.
The forward direction therefore composes two valid derivations with a grammar
production. The reverse direction follows from the CNF form: every derivation
of a non-unit span must have a root binary production and some split, both of
which are enumerated. Induction on span length establishes the invariant.

Backpointers are written only when a state first becomes reachable. Recursive
witness reconstruction follows those strictly shorter child spans and emits
nodes postorder, so replaying the returned witness against the original grammar
and input independently checks production membership, span partitioning,
terminal bytes, child order, and the complete root span.

## Independent evidence

The primary randomized oracle is deliberately not another bottom-up chart. A
memoized top-down recognizer starts from `(start, 0, n)` and recursively tries
all grammar productions and split points. It reaches only strictly shorter spans,
so its control flow and state-discovery order are structurally different from
production CYK.

Committed evidence includes:

- deterministic `S -> A B`, byte-terminal acceptance/rejection;
- arbitrary NUL and `0xff` terminals;
- explicit empty-word semantics;
- duplicate-production canonicalization and an ambiguous `S -> S S | 'a'`
  grammar whose repeated parse witness must be byte-for-byte identical and use
  the smallest split at the root;
- invalid zero-nonterminal, invalid start, and out-of-range production IDs;
- 1,200 fixed-seed random CNF grammars with 1-5 nonterminals, arbitrary-byte
  terminals from `{0x00,'a','b',0xff}`, and input lengths 0-6;
- production acceptance equality with the independent top-down oracle;
- independent replay of every accepted parse witness; and
- repeated-call equality for deterministic witness semantics.

Focused repo-native execution passed all four tests under strict GCC, strict
Clang, and an actual GCC ASan+UBSan build before upload.

## Complexity and non-claims

Let `Rt` be the number of canonical terminal productions, `Rb` the number of
canonical binary productions, `N` the nonterminal count, and `n` the input
length. This direct educational implementation uses `O(n * Rt + n^3 * Rb)` time
and `O(n^2 * N)` chart/backpointer state, plus `O(n)` witness nodes for an
accepted CNF parse tree. Production-rule canonicalization additionally costs
`O(Rt log Rt + Rb log Rb)`.

This slice does **not** claim a general CFG-to-CNF transformation, arbitrary
nullable/unit-production handling, parse-forest construction, ambiguity counts,
all-parse enumeration, Earley/LL/LR/GLR parsing, error recovery, incremental
parsing, practical parser-generator performance, or a sub-cubic CYK bound. The
recursive witness builder can use `O(n)` call-stack depth on a maximally skewed
parse; the dense chart intentionally favors transparent proof obligations over
sparse-grammar optimization.

## Scope

The intended review surface is exactly five files:

- one CMake source/test registration pair;
- `include/algorithms/automata/cyk_parser.hpp`;
- `src/automata/cyk_parser.cpp`;
- `tests/test_cyk_parser.cpp`; and
- this focused recovery proof document.

No README, ROADMAP, scope-recovery authority, workflow, benchmark, frozen
compiler/backend, coding-compression, or temporary-file churn is part of this
slice.
