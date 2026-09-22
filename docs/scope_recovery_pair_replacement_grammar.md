# Scope recovery: deterministic pair-replacement grammar

## Coverage decision

A fresh live-state audit after the distance-hereditary pruning checkpoint merged
as `main@729a0cbfc98ce7474593308398467113c0e73968` found zero open pull
requests and zero open issues. Default-branch code, branch names, and pull-request
history contained no pair-replacement grammar compressor, straight-line-program
compression surface, or occupied recovery branch for that capability.

The repository already contains LZ77, LZ78, and LZW. Those encode text through
back-references or adaptive phrase dictionaries. This slice deliberately changes
proof model again: it builds an acyclic binary rule DAG whose start sequence
contains terminals and nonterminals.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical ROADMAP remain untouched.

## Public contract

`pair_replacement_encode_bytes(input)` maps arbitrary bytes to a
`PairReplacementGrammar`.

- terminal symbols are exactly `0..255`;
- rule `i` receives nonterminal symbol `256+i`;
- each rule contains two symbols;
- rule `i` may reference only terminals or rules with smaller indices;
- the returned grammar is therefore acyclic by construction;
- the start sequence may contain terminals or any defined rule symbol;
- arbitrary bytes, including NUL and high-bit bytes, are ordinary terminals.

`pair_replacement_decode_bytes(grammar)` validates backward-reference shape,
checks all expanded lengths for `size_t` overflow, then expands the grammar
iteratively without recursive call-stack dependence.

`valid_pair_replacement_grammar` checks the same structural and expanded-size
safety boundary without producing output.

## Deterministic replacement policy

At every round production considers every adjacent symbol pair currently present
in the working sequence.

For each candidate pair it computes the number of **left-to-right
non-overlapping** occurrences. The selected pair is:

1. the candidate with the largest such count;
2. ties choose the lexicographically smaller symbol pair.

A rule is created only if the selected count is at least two.

Every selected occurrence is then replaced left-to-right by the new
nonterminal. Because every replacement consumes two symbols and emits one,
one round shortens the current sequence by exactly the selected non-overlapping
occurrence count.

This policy is intentionally explicit and replayable. It is not presented as
canonical Re-Pair behavior and does not claim to reproduce another library's
grammar.

## Acyclicity and exact decoding

Suppose rule `i` is created after rules `0..i-1`.

Every symbol in the current working sequence is either a byte terminal or a
nonterminal produced by an earlier round. Therefore both children of rule `i`
are terminals or symbols `256+j` with `j < i`.

Thus rule indices strictly decrease along every nonterminal edge. Cycles and
forward references are impossible for encoder-produced grammars.

For decoding, rule lengths are evaluated in increasing rule order. Since every
child length is already known, each rule length is exact. Checked addition
rejects an expansion whose size cannot be represented by `size_t`.

The decoder then expands the start sequence with an explicit symbol stack.
Pushing right then left reconstructs each binary rule in original order.
Induction over the backward rule DAG proves that expanding the returned start
sequence reproduces exactly the original byte sequence.

## Independent selection oracle

The primary randomized test oracle does not use production's ordered candidate
set.

Instead it visits every adjacent sequence position directly, deliberately
reconsiders duplicate candidate pairs, and for each occurrence performs a fresh
full-sequence scan to count left-to-right non-overlapping matches. It applies the
same public maximum-count / lexicographic policy and performs its own replacement
pass.

For every randomized input, the complete production grammar — every rule in
order and the final start sequence — must equal this brute-force oracle exactly.
Round-trip decoding alone is not considered sufficient evidence.

## Verification surface

Deterministic tests cover:

- empty input and literal-only input;
- a known repeated `abababab` grammar;
- equal-symbol overlap behavior on `aaaa`, proving non-overlap counting rather
  than overlapping-frequency counting;
- lexicographic tie breaking;
- arbitrary `0x00`, `0x80`, and `0xff` byte patterns;
- self/forward rule-reference rejection;
- unknown start-symbol rejection;
- checked expanded-size overflow rejection.

The randomized corpus uses 1,800 fixed-seed inputs of length `0..72`. Half are
drawn from a five-symbol alphabet to force repeated grammar structure; half use
the full byte alphabet. Every production result must equal the independent slow
selector, validate structurally, decode byte-for-byte, and reproduce
deterministically on repeated encoding.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

## Complexity / non-claims

Let the current sequence length be `m`.

A round has at most `m-1` distinct adjacent pairs. Production performs one
`O(m)` non-overlap scan per distinct pair, so one round is conservatively
`O(m^2)` plus ordered-candidate bookkeeping.

Every admitted rule replaces at least two non-overlapping occurrences and
therefore reduces sequence length by at least two. There are at most `O(n)`
rounds for input length `n`, giving the simple implementation a conservative
`O(n^3)` construction bound and `O(n)` resident grammar symbols/rules relative
to the working representation.

Decoding is linear in validated expanded output size plus grammar/start
bookkeeping.

This slice makes no claim of:

- minimum grammar size;
- canonical or specification-compatible Re-Pair output;
- entropy optimality;
- guaranteed serialized-size reduction;
- bit packing or file-format compatibility;
- streaming bounded-memory construction;
- asymptotically optimal pair-frequency maintenance;
- benchmark-backed speed or compression ratio.

The returned representation is an exact educational grammar structure, not a
compressed archive container.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/coding/pair_replacement_grammar.hpp`;
- `tests/test_pair_replacement_grammar_cases.hpp`;
- `docs/scope_recovery_pair_replacement_grammar.md`.

One pre-PR test-only fix parenthesizes an aggregate-literal assertion so the test
framework macro cannot misparse its comma; it does not change semantic coverage.

The isolated recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. No CMake, test-main, README, historical ROADMAP, workflow,
benchmark, frozen compiler/backend, or temporary-file change is required.

Base: `729a0cbfc98ce7474593308398467113c0e73968`.
