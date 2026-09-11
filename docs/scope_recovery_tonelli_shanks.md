# Scope recovery: exact prime-field square roots with Tonelli-Shanks

## Coverage decision

Fresh live code, pull-request history, and branch searches at
`main@ebf105eebadd3fa140dbe034a23ce2bdec3e869c` found no modular-square-root,
Tonelli-Shanks, or equivalent finite-field root-extraction capability. The
minimum-cycle-basis, general-graph-isomorphism, and linear-recurrence recovery
surfaces are already occupied, so this slice deliberately avoids those fronts.
It also changes proof model immediately after the merged SMAWK checkpoint rather
than farming another totally-monotone optimization primitive.

## Production contract

`algorithms::number_theory::tonelli_shanks_square_root(value, prime_modulus)`:

- requires a prime modulus and validates it through the sealed deterministic
  full-`uint64_t` primality routine;
- reduces `value` modulo the prime;
- returns the smaller canonical root `r` when `r^2 == value (mod p)`;
- returns `std::nullopt` for quadratic non-residues;
- handles `p=2` and the zero residue explicitly;
- supports the complete unsigned-64 prime domain without `__int128`, reusing the
  repository's overflow-safe `multiply_mod` and `power_mod` primitives.

The returned root is canonical only under the ordinary integer order on the two
field roots. No primitive-root, discrete-logarithm, composite-modulus square
root, higher-root, or factorization capability is implied.

## Algorithm / proof obligation

For an odd prime `p`, Euler's criterion first rejects non-residues. The easy
`p mod 4 == 3` case uses `a^((p+1)/4)`. Otherwise production factors
`p-1 = q * 2^s` with odd `q`, deterministically searches the smallest quadratic
non-residue `z >= 2`, and applies Tonelli-Shanks to the state `(x,t,c,m)`.

The loop invariant is that `x^2 == a*t (mod p)`, `t` lies in the `2^m`-torsion
subgroup, and `c` has the required `2^m` order role. Choosing the least `i` with
`t^(2^i)=1` and multiplying by the prescribed power of `c` strictly decreases
`m`; termination with `t=1` therefore leaves `x^2 == a`.

Correctness relies on Euler's criterion, cyclicity of the multiplicative group of
a finite prime field, and the standard Tonelli-Shanks invariant. Tests are
implementation evidence, not substitutes for those theorems.

## Verification

Focused pre-upload verification passed under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

The primary small-domain oracle is independent brute-force root enumeration.
For every prime `p <= 251` and every residue class `0 <= a < p`, production must
exactly match the first brute-force root, which is the canonical smaller root.
Deterministic regressions cover composite-modulus rejection, `p=2`, zero,
`p mod 4 == 3`, the general `p mod 4 == 1` path, non-residues, the NTT prime
`998244353`, and the full-width prime `18446744073709551557`.

The large-prime square witnesses are replayed through the sealed overflow-safe
modular multiplication. A fixed full-width non-residue regression exercises the
`std::nullopt` path without relying on floating-point or wider-integer arithmetic.

## Complexity / non-claims

Let `p-1 = q*2^s`, and let `j` be the number of deterministic candidates tested
before the first quadratic non-residue is found. Counting modular
multiplications/exponentiations, the search costs `O(j log p)` modular
multiplications and the Tonelli-Shanks phase uses `O(s^2 + log p)` modular
multiplications. In this repository each `multiply_mod` itself uses `O(log p)`
word-level additions/doublings, so the code-local bound is correspondingly
larger by that factor.

No constant or logarithmic worst-case bound is claimed for the deterministic
non-residue scan, and no cryptographic constant-time / side-channel property is
claimed.

## Scope

Exactly three files change: one header-only production API, appended native
number-theory tests in the already-registered test translation unit, and this
proof document. No CMake, README, ROADMAP, recovery-authority, workflow,
benchmark, compiler/backend, or occupied recovery-surface churn is included.
