# Scope recovery: suffix automaton byte index

## Capability

This recovery slice adds a first-principles suffix automaton over arbitrary byte
strings. It is deliberately distinct from the sealed suffix-array/BWT/FM-index
family and from the later Aho-Corasick multi-pattern matcher: the indexed object
is one text, and the structural invariant is the partition of substrings by
suffix-link / `endpos` equivalence.

The public index provides:

- exact substring membership;
- exact overlapping occurrence counts;
- exact distinct non-empty substring count;
- observable automaton state count and source text length for bounded structural
  verification.

The empty pattern is defined to occur at every text boundary, so a text of length
`N` has `N+1` empty-pattern occurrences. Arbitrary bytes, including NUL and
high-bit values, are accepted without text encoding assumptions.

## Construction invariants

For every non-root state `v`:

- `len(link(v)) < len(v)`;
- the state represents substring lengths
  `(len(link(v)), len(v)]` sharing the same `endpos` set;
- a normal extension state contributes one new terminal end position;
- a clone copies transitions and the old suffix link but starts with zero direct
  terminal occurrences because it does not correspond to a newly consumed text
  position;
- when an extension would violate the one-character suffix-link length relation,
  redirecting transitions to a clone restores deterministic equivalence classes.

After construction, occurrence counts are propagated from longer states to their
suffix links in descending `len` order. This yields the cardinality of each
state's `endpos` set. The number of distinct non-empty substrings is

`sum_v!=root (len(v) - len(link(v)))`.

All aggregate `size_t` additions are checked; overflow is rejected rather than
wrapped.

## Complexity

With text length `N`, pattern length `M`, and byte alphabet size `sigma <= 256`:

- construction creates at most `2N` states for non-empty input;
- sparse `std::map` transition lookup/update costs `O(log sigma)`, while cloning
  copies the target state's sparse transition set; a conservative bound for
  construction plus post-processing is `O(N log N + N*sigma)` for this direct
  implementation, with fixed byte `sigma <= 256`;
- substring queries cost `O(M log sigma)`;
- occurrence propagation sorts states by length in `O(N log N)`;
- storage is `O(N*sigma)` in the conservative worst case and `O(N)` states.

No textbook strict-linear construction claim is made for this sparse-map
implementation. `std::sort` is a supporting primitive for post-construction
length order, not a library implementation of the automaton.

## Verification

Focused repository-style GCC strict-warning, Clang strict-warning, and actual
GCC ASan+UBSan builds pass the complete slice tests.

Deterministic cases cover empty text/pattern, `banana`, repeated-symbol strings,
embedded NUL/high-bit bytes, and repeated construction/query determinism.

The primary randomized oracle is structurally independent: 900 fixed-seed byte
texts of length 0-18 enumerate every substring directly into a
`std::map<string,count>`. The test compares every enumerated substring's
membership and multiplicity, the exact number of distinct substrings, additional
random query patterns against a naive direct scan, and the classical `<= 2N`
state bound. The oracle does not use suffix links, clones, or automaton
transitions.

## Non-claims

This slice does not add online text extension to the public API, lexicographic
substring enumeration, longest-common-substring queries, compressed transition
storage, or a replacement for the sealed suffix-array/BWT/FM-index capabilities.
The goal is the suffix-link / clone / endpos proof model itself, not string-index
feature count.
