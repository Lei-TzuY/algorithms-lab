# Scope recovery: Aho-Corasick byte multi-pattern matching

## Capability

This recovery slice adds an exact multi-pattern matching automaton for arbitrary
byte strings. `AhoCorasickByteMatcher` builds one trie over the supplied pattern
sequence, computes failure links breadth-first, and completes a dense 256-symbol
goto table so each scanned byte advances in constant time.

Pattern identity is the input index. Duplicate patterns therefore remain distinct
observable matches rather than being deduplicated. Empty patterns are supported
and match every text boundary, consistent with the repository's explicit empty-
pattern search semantics. Embedded NUL and high-bit bytes are ordinary symbols;
all transition indices convert through `unsigned char` before indexing.

`find_all()` returns half-open `[begin,end)` witnesses in deterministic order:
text end position increases first; at one end position, longer matching suffixes
are emitted before shorter ones, and duplicate/equal-length patterns keep input
index order.

## Invariants and proof obligations

For every automaton state, its trie path spells a prefix of at least one pattern.
Its failure link names the longest proper suffix of that path that is also a trie
prefix. Breadth-first construction guarantees the failure state's complete goto
row already exists before a child row is resolved.

An output link skips directly to the nearest failure ancestor that is terminal.
Following output links therefore enumerates exactly the terminal suffixes of the
current trie path, in strictly decreasing pattern length. Terminal pattern IDs at
a single trie state are inserted in input order, which supplies deterministic
ordering for duplicate patterns.

During scanning, the completed goto transition preserves the longest suffix of
the consumed text that is a trie prefix. Emitting the current terminal state plus
its output-link chain therefore reports every pattern ending at the current text
boundary exactly once. Root-terminal empty patterns are emitted at boundary zero
and, through output links, after every consumed byte.

## Complexity

Let `P` be the pattern count, `S` the total number of pattern bytes, `Q` the
number of trie states, `N` the text length, and `Z` the number of reported
matches. Trie insertion and terminal-ID registration cost `O(S + P)`. Completing
all byte transitions is `O(256 * Q)`. Search is `O(N + Z)` because each byte
performs one table lookup and output links visit only terminal suffix states that
emit matches.

The direct fixed-byte-alphabet representation uses `O(256 * Q + P)` storage.
This deliberately trades memory for a simple constant-time transition contract;
it is not presented as a compressed or sparse automaton.

## Verification

Deterministic tests cover the classic `he/she/his/hers` failure-link example,
overlapping patterns, duplicate pattern identities, empty patterns, repeated
queries, no-pattern input, embedded NUL, and bytes `0x80`/`0xff`.

A fixed-seed differential corpus generates 700 small arbitrary-byte pattern sets
and texts. An independent naive oracle checks every pattern at every text
boundary, sorts only according to the documented result-order contract, and
compares the entire emitted witness sequence. Every production witness is also
replayed against the original text bytes.

Focused GCC and Clang strict-warning builds and an actual ASan+UBSan build pass
this suite before the remote candidate is created. Full repository regression is
still required on the exact pull-request head before integration.

## Scope boundary

This slice does not modify the sealed BWT/FM-index family and does not implement
regex syntax, wildcard transitions, approximate matching, dynamic pattern
insertion, or a Unicode code-point layer. Those would be different abstractions.
The value here is the missing classical failure-automaton model and its exact
multi-pattern output semantics, not another BWT variant.
