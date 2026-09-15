# Scope recovery: 64-bit Myers Levenshtein bit vectors

## Coverage decision

Fresh live recovery audit at `main@791c7dac364727905c6c595dae510e326f46b3d2`
found no Myers bit-vector / bit-parallel edit-distance implementation in the
default branch, pull-request history, or branch namespace. The authoritative
prospective policy remains `scope_recovery_after_phase69.md`; the frozen
compiler/backend ROADMAP tail is not resumed.

This slice deliberately changes proof/state representation after the recent
soft-heap, quotient-filter, disjoint-sparse-table, and GF(2) linear-basis
checkpoints. The repository already has a full-table Levenshtein implementation
with edit-script reconstruction; the subject here is the single-machine-word
vertical-difference encoding of the same exact dynamic program.

## Production contract

`algorithms::strings::myers_levenshtein_distance_64(pattern, text)` computes
unit-cost Levenshtein distance over arbitrary byte strings.

- `pattern.size() <= 64` is required; longer patterns throw
  `std::length_error` rather than silently switching algorithms;
- text length is not subject to the 64-byte bit-parallel bound;
- empty pattern returns `text.size()`;
- all 256 byte values, including NUL and high-bit bytes, are ordinary symbols;
- the result exposes the exact distance plus final positive/negative vertical
  bit vectors so the compressed DP state is replayable.

The API is intentionally asymmetric because the caller chooses which operand is
the bounded bit-parallel pattern. Levenshtein distance itself remains symmetric.

## Bit-vector invariant

Let `D[i,j]` be the ordinary Levenshtein distance between `pattern[0,i)` and
`text[0,j)`. After consuming `text[0,j)`, bit `i` (`0 <= i < m`) of `Pv` is set
exactly when

`D[i+1,j] - D[i,j] = +1`,

while the corresponding bit of `Mv` is set exactly when the difference is `-1`.
If both bits are clear, the difference is zero. `Pv` and `Mv` are disjoint.
The maintained scalar score is `D[m,j]`.

For the next text byte, `Eq` marks pattern rows whose symbol matches that byte.
The Myers horizontal/vertical bit recurrence computes all next-column edit
choices in parallel, updates the bottom-row score from the pattern high bit,
then reconstructs the next `Pv/Mv` difference masks. This is the word-parallel
form of the ordinary insertion/deletion/substitution recurrence; the classical
Myers bit-vector derivation is the proof obligation, not something inferred from
finite testing.

The unsigned addition inside the recurrence intentionally uses modulo-`2^64`
carry behavior. Public numeric costs are not represented by that wrapped word.
For patterns shorter than 64 bytes, all state is masked back to the live pattern
bits after every recurrence step.

## Verification

Focused exact candidate verification passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers empty and known textbook distances, arbitrary
`0x00`/`0x80`/`0xff` bytes, the exact 64-byte pattern boundary, and 65-byte
pattern rejection.

The primary randomized oracle is structurally independent of the bit-vector
recurrence: 1,600 fixed-seed pattern/text pairs use an ordinary two-row scalar
Levenshtein DP. Production must match the exact final distance. Tests also retain
the oracle's complete final DP column and verify every returned `Pv/Mv` bit
against the corresponding scalar vertical difference. When both operands fit
64 bytes, the same corpus additionally checks symmetry by swapping operands.

The existing full-table edit-distance implementation is not used as the primary
oracle.

## Complexity and non-claims

For pattern length `m <= 64` and text length `n`, preprocessing performs
`O(256 + m)` fixed-word work and scanning performs `O(n)` fixed-word work.
Auxiliary storage is 256 machine words plus constant state.

This is deliberately a one-word educational baseline. It does not claim:

- multiword Myers support for patterns longer than 64 bytes;
- edit-script reconstruction;
- approximate substring-search semantics;
- SIMD/vectorized acceleration or benchmark speedup;
- a replacement for the existing full-table implementation when reconstruction
  or an unbounded pattern is required.
