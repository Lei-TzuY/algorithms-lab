# Scope recovery: insertion-only dynamic Aho dictionary

## Coverage decision

A fresh live-state audit after incremental transitive closure merged as
`main@e875b7838a2fe6aa66743959306acca66c742057` found zero open pull
requests and zero open issues. Default-branch code, branch names, and pull-request
history contained no dynamic Aho-Corasick / online pattern-insertion dictionary.

The sealed static `AhoCorasickByteMatcher` explicitly lists dynamic pattern
insertion outside its scope. This recovery therefore changes the state model
rather than adding another static query wrapper: patterns arrive online, prior
patterns remain active, and queries search the complete current dictionary.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical roadmap remain untouched.

## Production contract

`IncrementalAhoCorasickByteDictionary` is an insertion-only dictionary of
arbitrary byte patterns.

- `add_pattern(pattern)` assigns one stable global pattern id in insertion
  order;
- duplicate patterns remain distinct ids;
- empty patterns are supported and match every text boundary;
- all byte values, including `0x00`, `0x80`, and `0xff`, are ordinary
  symbols;
- all previously inserted patterns remain active forever;
- `find_all(text)` reports exact half-open `[begin,end)` witnesses using
  global pattern ids;
- result order is deterministic: increasing end boundary, then increasing begin
  boundary (longer matches first at one end), then increasing global pattern id;
- `pattern_count()`, `active_bucket_count()`, and
  `valid_invariants()` expose state/diagnostics.

Pattern deletion, replacement, wildcard/regex syntax, approximate matching,
Unicode code-point semantics, concurrent mutation, and streaming text state
across separate queries are outside this slice.

## Logarithmic-method representation

Production does not mutate failure links after every insertion. Instead it uses
the standard logarithmic method / binary-counter decomposition over immutable
static Aho-Corasick automatons.

Bucket level `i`, when occupied, contains exactly `2^i` patterns. Therefore
the occupied levels encode the binary representation of the total pattern count,
and at most `floor(log2(P))+1` automatons are active after `P` insertions.

To insert one new pattern:

1. find the first empty bucket level `L`;
2. collect the older pattern-id ranges from occupied levels
   `L-1 .. 0`, then append the new pattern;
3. build one new immutable `AhoCorasickByteMatcher` for that complete
   `2^L`-pattern carry;
4. only after the rebuild succeeds, clear lower buckets and publish the new
   level-`L` bucket.

The reverse-level collection order preserves monotonically increasing global
pattern ids inside every rebuilt matcher.

## Transactional rebuild boundary

All copying, string allocation, vector growth, and static-automaton construction
for a carry occur before resident buckets are modified. If any of those
operations throws, the existing dictionary remains unchanged.

After the candidate matcher exists, the only state transition is resetting
lower optionals and moving the completed bucket into the first empty level.
Bucket state is composed of ordinary vectors plus a `unique_ptr` to the static
matcher; no existing automaton is partially mutated.

This gives insertion a strong semantic boundary with respect to allocation or
static-matcher construction failure.

## Query invariant and global ordering

Every global pattern id occurs in exactly one occupied bucket. Searching all
occupied static automatons therefore enumerates every active pattern match
exactly once.

Each local static match maps its matcher-local pattern index through the
bucket's global-id vector. The union is then sorted by:

1. `end`;
2. `begin`;
3. global `pattern_id`.

For matches sharing one end boundary, smaller begin means the longer suffix and
therefore reproduces the sealed static matcher's longer-before-shorter contract.
Equal begin/end witnesses are duplicate/equal patterns and global insertion id
provides deterministic order. Empty patterns naturally appear after non-empty
matches at the same positive boundary.

## Complexity / non-claims

Let `P` be the current pattern count, `S` the total number of bytes in all
inserted patterns, `N` the query text length, and `Z` the emitted match count.

At most `O(log(P+1))` static automatons are active. Each pattern participates
in at most one rebuild per binary-counter level, so across a sequence of
insertions each pattern byte is rebuilt `O(log(P+1))` times. With the sealed
dense 256-symbol static Aho representation, total rebuild work over a growing
dictionary is conservatively bounded by the corresponding
`O(256 * (S + P) * log(P+1))` state/transition-scale work.

One query scans the text once per occupied bucket, then globally orders emitted
witnesses:

- matcher scanning: `O(N log(P+1) + Z)`;
- final deterministic ordering: `O(Z log Z)`;
- resident pattern/string plus automaton storage: `O(256 * (S + P))` in the
  fixed-alphabet dense representation, up to constant factors across occupied
  buckets.

A single insertion can still trigger a large carry and rebuild `Theta(P)`
patterns; no worst-case logarithmic insertion claim is made. No benchmark,
compressed-automaton, deletion, deamortized rebuild, or lock-free claim is made.

## Independent verification

The committed test oracle stores the current patterns as ordinary strings and,
for each text boundary, directly checks every pattern with substring comparison.
It never uses trie states, failure links, binary buckets, or the production
logarithmic rebuild recurrence.

Deterministic coverage includes:

- an empty dictionary;
- all binary carry levels through 128 online insertions;
- the classic `he/she/his/hers` family;
- duplicate and empty patterns inserted at different times;
- arbitrary bytes including NUL and high-bit values;
- continuous structural replay and active-bucket/popcount agreement.

The randomized corpus performs 50 fixed-seed online dictionaries with mixed
insertion/query traces, pattern lengths `0..6`, text lengths `0..40`, a
small collision-heavy byte alphabet plus `0x00/0x80/0xff`, and compares the
complete globally ordered witness vector against the naïve oracle.

A direct container checkout was unavailable because that environment could not
resolve GitHub. No local-compiler success is claimed from that failed attempt.
Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the pull
request head are required before integration.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/strings/incremental_aho_dictionary.hpp`;
- `tests/test_incremental_aho_dictionary_cases.hpp`;
- `docs/scope_recovery_incremental_aho_dictionary.md`.

The isolated recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. The implementation reuses the already-linked sealed static
Aho-Corasick implementation, so no new production source registration is needed.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Base: `e875b7838a2fe6aa66743959306acca66c742057`.
