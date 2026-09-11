# Scope recovery: Berlekamp-Massey linear recurrences

## Coverage decision

A fresh live-code, branch, pull-request, and recovery-document audit at
`main@5555c6265891f8858f63957cb5b67994409d8c9e` found no Berlekamp-Massey
implementation, no minimal linear-recurrence synthesis API, and no fast nth-term
linear-recurrence evaluator. The historical branch named
`scope-recovery-linear-recurrence` is stale: it still points to the old suffix
automaton merge checkpoint rather than recurrence work, so this slice uses the
fresh `scope-recovery-berlekamp-massey` branch instead of rewriting that history.

This slice follows `scope_recovery_after_phase69.md`: it leaves the frozen
compiler/backend frontier and stale historical ROADMAP heading untouched. It also
moves away from the immediately preceding FKS perfect-hashing slice rather than
farming adjacent hashing variants.

## Production contract

`berlekamp_massey(sequence, prime_modulus)` accepts canonical residues in the
prime field `F_p` and returns the minimum-order recurrence

`S[n] = c[0] S[n-1] + ... + c[L-1] S[n-L] (mod p)`.

The empty sequence and an all-zero sequence have order zero. Composite moduli and
non-canonical residues are rejected explicitly.

`linear_recurrence_nth(initial, coefficients, index, prime_modulus)` evaluates a
term through characteristic-polynomial binary exponentiation rather than linear
rollout. An order-`L` recurrence requires at least `L` canonical initial terms.
An order-zero recurrence admits only zero initial data and evaluates to zero.

Both operations reuse the sealed overflow-safe `multiply_mod`, deterministic
full-`uint64_t` primality test, and Fermat inverse through `power_mod`; no native
multiplication-overflow shortcut is introduced.

## Berlekamp-Massey proof boundary

The production connection polynomial starts at `C(x)=1`. At each observed index,
the discrepancy is the supplied sequence value plus the current connection
polynomial's history terms, equivalently `actual - predicted` under the returned
recurrence convention. A zero discrepancy leaves the recurrence unchanged. A
non-zero discrepancy subtracts an appropriately shifted, scaled previous
connection polynomial; the scale is `d / b` in `F_p`, where `b` is the last
discrepancy that increased the linear complexity.

When the old linear complexity `L` satisfies `L <= floor(n/2)`, the standard
Berlekamp-Massey update raises the new complexity to `n+1-L` and records the old
connection polynomial as the next correction basis. The implementation uses this
overflow-safe comparison rather than computing `2*L`.

The Berlekamp-Massey theorem over a field establishes that the resulting
connection polynomial has minimum possible order for the processed prefix. Tests
exercise that property independently on small fields, but the theorem is a proof
obligation rather than something inferred from finite random testing.

## Fast nth-term proof boundary

For coefficients `c[0..L-1]`, define the characteristic relation

`x^L = c[0]x^(L-1) + ... + c[L-1]`.

Polynomial multiplication followed by reduction with this relation preserves
the represented sequence-shift operator. Binary exponentiation computes
`x^index` modulo the characteristic polynomial; dotting the resulting degree
`< L` coefficients with the first `L` sequence terms yields `S[index]`.

The implementation keeps temporary polynomial degree below `2L-1`. Workspace
length checks use overflow-safe `max/2 + 1` arithmetic. Focused verification
caught and fixed an earlier `(max+1)/2` formulation whose unsigned wraparound
would have rejected every non-zero recurrence; that boundary remains executable
through the production guard and sanitizer builds.

## Complexity and non-claims

Let `N` be the observed prefix length and `L` the discovered recurrence order.

- Berlekamp-Massey uses `O(NL)` field operations and `O(L)` resident recurrence
  state in this direct first-principles implementation; the usual `O(N^2)` field-
  operation upper bound follows from `L <= N`.
- nth-term evaluation uses `O(L^2 log index)` field multiplications/additions and
  `O(L)` result/base state plus `O(L)`-scale temporary reduced-polynomial storage.
- repository `multiply_mod` itself is overflow-safe repeated doubling rather than
  a native constant-time word multiply, and Fermat inversion uses modular
  exponentiation. No stronger machine-level timing claim is implied.

No composite-ring Berlekamp-Massey, rational reconstruction, sparse recurrence,
fast polynomial-reduction/NTT acceleration, cryptographic claim, or benchmark-
backed performance claim is included in this slice.

## Verification

Focused repo-native verification passed before upload under GCC C++20 strict
warnings-as-errors, Clang C++20 strict warnings-as-errors, and actual GCC
ASan+UBSan with leak detection.

Deterministic cases cover empty/all-zero sequences, Fibonacci and geometric
recurrences, composite-modulus rejection, non-canonical residues, insufficient
initial state, and order-zero validation before known-term fast paths.

For minimum-order evidence, 240 fixed-seed length-8 sequences over `F_5` are
checked against an independent exhaustive oracle that enumerates every
coefficient vector at every smaller order until it finds the first fitting
recurrence. Production must both replay the sequence and return exactly that
minimum order.

For extrapolation evidence, 240 fixed-seed recurrences of orders 1 through 6 over
prime `1,000,003` generate 160 terms by direct recurrence rollout. Production sees
only the first 40 terms, synthesizes a recurrence, and must reproduce held-out
terms at indices 40, 57, 100, and 159 through characteristic-polynomial
exponentiation.

## Scope

Exactly four repository files change: one header-only production implementation,
a dedicated native test translation unit, the single CMake test registration line,
and this recovery proof document. No README, ROADMAP, scope-recovery authority,
workflow, benchmark, or frozen compiler/backend file changes.
