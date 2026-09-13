# Scope recovery: linear-time Manacher palindrome radii

## Coverage decision

A fresh live audit at `main@163b6a6851e4571570e4d9691ed837d103fde3eb`
found no Manacher implementation in default-branch code, pull-request history,
or the branch namespace. The repository already contains an online arbitrary-byte
palindromic tree (Eertree), whose focused recovery document explicitly excludes
Manacher radii. This slice therefore adds a different palindrome proof model
rather than another wrapper around the existing Eertree.

The sealed Eertree indexes distinct palindromes incrementally through suffix links
and transitions. Manacher instead performs one immutable linear scan and returns
the exact maximal palindrome radius around every odd and even center. Those radii
encode all palindromic occurrences, not only distinct palindromes, and expose a
mirror/right-boundary invariant absent from the Eertree surface.

Prospective governance remains `docs/scope_recovery_after_phase69.md`; the frozen
compiler/backend excursion and historically stale `ROADMAP.md` headings do not
drive this recovery work.

## Production contract

`algorithms::strings::manacher_palindrome_radii(bytes)` accepts an arbitrary-byte
`std::string_view` and returns:

- `odd_radius[i]`: the maximal radius including center `i`, describing the exact
  palindrome `[i-r+1, i+r)`;
- `even_radius[i]`: the maximal radius on each side of the boundary before `i`,
  describing `[i-r, i+r)`;
- one canonical longest-palindrome witness `(longest_start, longest_length)`,
  breaking equal-length ties toward the smallest start offset.

Empty input returns empty radius arrays and longest witness `{0,0}`. Byte equality
is exact, so embedded NUL and high-bit bytes have no special semantics. The call
rejects only a theoretical input length whose doubled radius/length representation
would exceed `size_t`; ordinary valid inputs have no content precondition.

## Mirror-window invariant

For each parity, production keeps the rightmost already-known palindrome as a
half-open interval `[left,right)`.

For a new center inside that interval, reflection across the palindrome center
maps it to an already-processed mirror center. The mirror radius can therefore be
reused up to the current right boundary without re-comparing those bytes. Only
comparisons that touch or extend beyond `right` can advance the right boundary.
The algorithm then expands directly from that safe initial radius and replaces
the tracked interval only when the new palindrome extends farther right.

Every successful comparison that is not inherited from a mirror either advances
the current center locally or advances the globally monotone right boundary.
Across the odd pass and even pass the total comparison work is linear, yielding
`O(n)` time and `O(n)` result storage.

The classical Manacher mirror argument is the proof obligation. Test timings are
not used as a complexity proof.

## Independent verification

Focused exact-candidate verification passes:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection / halt-on-error: 4/4.

Deterministic cases cover empty/singleton input, pure odd and even palindromes,
leftmost tie selection, repeated bytes, non-palindromic boundaries, embedded NUL,
and high-bit byte values.

The primary oracle is independent center expansion. For 1,200 fixed-seed random
byte strings of length `0..160`, it expands every odd and even center directly
until the first mismatch/boundary and compares both complete radius vectors plus
the canonical longest witness exactly. Additional replay checks verify every
returned radius describes a palindrome and is maximal at its center.

The oracle contains no mirror/window reuse and does not call the Eertree. Finite
random testing is implementation evidence, not the proof of the linear-time
Manacher invariant.

## Complexity and non-claims

The returned arrays themselves require `O(n)` space. The direct implementation
uses `O(1)` additional scalar state beyond those outputs and runs in `O(n)` time.

This slice does not add an online palindrome index, distinct-palindrome counting,
palindromic factorization/partition DP, approximate palindrome matching, dynamic
deletion, or a claim that Manacher replaces the existing Eertree. It adds the
missing exact all-center radius representation and its linear mirror invariant.

## Scope

Exactly four paths are intended to differ from live main:

- `include/algorithms/strings/manacher.hpp`;
- `tests/test_manacher_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused proof/coverage document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or unrelated recovery surface is changed.
