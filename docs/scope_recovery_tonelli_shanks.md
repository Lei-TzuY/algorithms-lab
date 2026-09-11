# Scope recovery: exact prime-field square roots via Tonelli-Shanks

## Coverage decision

A fresh live audit at `main@ebf105eebadd3fa140dbe034a23ce2bdec3e869c` found no Tonelli-Shanks, modular-square-root, or equivalent prime-field square-root capability in live code, PR history, or active branches. Several other recovery surfaces are already occupied — notably minimum cycle basis, general graph isomorphism, and linear recurrence — so this slice deliberately avoids competing with them.

The slice also changes proof model after the merged SMAWK totally-monotone row-minimum checkpoint. It reuses the sealed deterministic primality and overflow-safe modular-arithmetic substrate, but it is not another discrete-logarithm or factorization wrapper.

## Production contract

`algorithms::number_theory::tonelli_shanks_square_root(value, prime_modulus)`:

- requires a prime modulus and validates it through the sealed deterministic `is_prime` implementation;
- reduces `value` modulo the prime;
- handles zero and the field `F_2` explicitly;
- returns `std::nullopt` for quadratic non-residues;
- for a quadratic residue, returns the smaller canonical root `min(r, p-r)`;
- uses the `p % 4 == 3` shortcut when available;
- otherwise runs deterministic Tonelli-Shanks, selecting the smallest quadratic non-residue by ascending search;
- reuses `multiply_mod` / `power_mod`, so no wider integer type or unchecked multiplication is introduced.

## Correctness / proof boundary

Euler's criterion distinguishes nonzero quadratic residues from non-residues in a prime field. For odd prime `p`, write `p-1 = q * 2^s` with odd `q`. Tonelli-Shanks maintains the standard invariant that the current `root` and `residue` encode the target square-root problem inside the shrinking 2-primary subgroup; locating the least exponent that sends the current residue to one and multiplying by an appropriate power of a quadratic non-residue strictly decreases the active 2-adic exponent until the residue becomes one.

Correctness therefore relies on the cyclic structure of `F_p^*`, Euler's criterion, and the classical Tonelli-Shanks invariant. Tests are implementation evidence rather than substitutes for those theorems.

## Verification

Focused pre-upload verification passed under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan.

Deterministic coverage includes composite-modulus rejection, `p=2`, zero, `p % 4 == 3`, a general Tonelli-Shanks case, explicit non-residues, a square over `998244353`, and a full-`uint64_t` prime near `2^64`.

The primary oracle is structurally independent of Tonelli-Shanks: for every prime `p <= 251` and every residue class `a in [0,p)`, tests linearly scan all candidate roots and compare the exact canonical result. Full-width vectors use the sealed overflow-safe multiplication routine only to construct the square whose root is then recovered.

## Complexity / non-claims

Let `p-1 = q * 2^s`, and let `j` be the first integer at least two that the deterministic search identifies as a quadratic non-residue. With modular exponentiation treated in the usual fixed-word arithmetic model, the non-residue search costs `O(j log p)` modular multiplications and the Tonelli-Shanks loop costs `O(s^2 + log p)` modular multiplications.

Code-locally, this repository's first-principles `multiply_mod` itself uses `O(log p)` modular additions/doublings, so arithmetic cost inherits that factor.

No constant-time or cryptographic implementation claim is made. The deterministic ascending non-residue search is not claimed to have a constant or logarithmic worst-case bound in the candidate value `j`. No composite-modulus square root, Hensel lifting, CRT composition, or arbitrary-precision claim is implied.

## Scope

Exactly three files change:

- `include/algorithms/number_theory/modular_square_root.hpp`;
- the existing `tests/test_number_theory.cpp`;
- this focused recovery proof document.

No CMake, README, ROADMAP, recovery-authority, workflow, benchmark, compiler/backend, or occupied recovery-branch surface changes.
