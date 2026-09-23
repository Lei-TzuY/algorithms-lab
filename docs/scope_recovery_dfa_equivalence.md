# Scope recovery: DFA equivalence witness

## Coverage

Fresh audit after `main@d1af4489c038ff7d66ad8200ae3c732fc6e9e81a` found no open PR,
no open issue, no DFA-equivalence implementation, no matching branch, and no
prior implementation PR.

The repository already has complete-DFA minimization and epsilon-NFA
determinization. This slice reuses the existing `algorithms::automata::Dfa`
type and adds direct language comparison plus a concrete counterexample word.

## Contract

`compare_dfa_languages(first, second, max_product_states)` compares two complete
DFAs over the same explicit symbol-index alphabet.

Inputs must have:

- at least one state;
- an in-range start state;
- one accepting bit per state;
- rectangular transition rows;
- in-range transition targets;
- equal alphabet sizes.

The alphabet may be empty. `max_product_states` must be positive.

The result exposes:

- `equivalent`;
- `distinguishing_word`;
- `explored_product_states`.

For inequivalent DFAs, the word is shortest, and among shortest witnesses it is
lexicographically smallest by symbol index. The empty word is valid when the two
start states already disagree on acceptance.

## Algorithm

Production performs BFS on reachable pairs `(p,q)` of DFA states.

For each pair, symbols are expanded in ascending index order. First discovery of
a pair records its predecessor id and incoming symbol. The first pair whose
accepting bits differ ends the search; predecessor links reconstruct the word.

The visited structure is an ordered map keyed by state pairs, so only reachable
product states are materialized. Production throws `std::length_error` before
adding a state that would exceed the caller-provided budget.

## Correctness

BFS processes nondecreasing word length. Ascending-symbol expansion makes the
first path to each pair lexicographically smallest among shortest paths to that
pair.

If two different words reach the same pair, every future suffix produces the
same continuation from that pair. Keeping only the first path therefore cannot
remove a shorter or lexicographically better distinguishing continuation.

Thus the first accepting-disagreeing pair gives the shortest witness, with the
required tie break.

If BFS exhausts all reachable pairs without an acceptance disagreement, every
input word ends in a pair whose accepting bits agree, so the start-state
languages are equal.

## Independent oracle

Tiny randomized tests do not use product-state visited search.

They directly enumerate all words by:

1. increasing length;
2. lexicographic symbol order within each length.

For DFAs with `n` and `m` states, a shortest distinguishing word has length
at most `n*m-1`: a shortest path to a disagreeing product state never needs to
repeat a product state.

The tiny oracle therefore checks every word through that bound and returns the
first mismatch.

Committed coverage includes:

- identical DFAs;
- empty alphabets;
- empty-word inequivalence;
- one-symbol tie breaking;
- a fixed two-symbol witness `[0,1]`;
- unreachable-state differences;
- product-state budget exhaustion;
- malformed DFAs and alphabet mismatch;
- 800 fixed-seed random pairs with 1..3 states per side and alphabet size 0..2,
  checked exactly against complete word enumeration.

A supplementary model checked 10,000 additional random tiny pairs with zero
mismatch.

## Complexity

Let `R` be the number of reachable product states explored, `K` the alphabet
size, and `L` the witness length.

The ordered visited map gives conservative time
`O(K * R * log R + L)` and resident storage `O(R + L)`.

## Non-claims

This slice does not add DFA minimization, NFA equivalence, symbolic alphabets,
weighted automata, regex equivalence, or performance benchmark claims.

## Scope

Exactly three paths are intended:

- `include/algorithms/automata/dfa_equivalence.hpp`;
- `tests/test_dfa_equivalence_cases.hpp`;
- `docs/scope_recovery_dfa_equivalence.md`.

Exact GCC release, Clang release, and GCC ASan+UBSan CI must be green before
integration.
