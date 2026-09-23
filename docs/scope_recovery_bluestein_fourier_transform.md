# Scope recovery: arbitrary-length Bluestein Fourier transform

## Coverage decision

After the vEB-layout static ordered set merged as
`main@1f896af9cc2edca47a112efba168bdd54ef0e225`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains a first-principles radix-2 complex FFT whose
contract deliberately rejects non-power-of-two lengths and explicitly lists
arbitrary-length Bluestein transforms as a non-claim.

No Bluestein / chirp-z / arbitrary-length FFT implementation, branch, or prior
implementation PR was present at branch creation.

This slice therefore extends an existing numerical substrate across a real
capability boundary rather than replacing one FFT algorithm with another.

## Public contract

`bluestein_fourier_transform(values, inverse)` is an in-place transform over
`Complex64 == std::complex<double>`.

- empty input is a no-op;
- every non-empty length whose checked convolution span and padded power-of-two
  workspace are representable is accepted;
- inputs must have finite real and imaginary components;
- forward convention is
  `exp(-2*pi*i*k/n)`;
- inverse uses the opposite sign and divides by `n`;
- power-of-two lengths delegate to the sealed radix-2 FFT;
- other lengths use Bluestein chirp convolution.

The sign and normalization contract therefore matches
`fast_fourier_transform` exactly.

## Bluestein reduction

For sign `s`, where forward uses `s=-1` and inverse uses `s=+1`,

`exp(s*2*pi*i*n*k/N)`

is rewritten using

`2*n*k = n^2 + k^2 - (n-k)^2`.

Define

- `a[n] = x[n] * exp(s*pi*i*n^2/N)`;
- `b[d] = exp(-s*pi*i*d^2/N)`.

Then the linear convolution of `a` and `b`, sampled at output index `k`,
multiplied by `exp(s*pi*i*k^2/N)`, equals the requested DFT term.

Production embeds the symmetric chirp kernel into a zero-padded circular
convolution buffer of power-of-two length at least `2*N-1`, then reuses the
existing radix-2 FFT for forward transforms, pointwise multiplication, and the
inverse convolution transform.

For inverse DFT, the final values are additionally divided by `N`.

## Numerical phase construction

The chirp phase is evaluated in `long double` before conversion to
`complex<double>`.

The square phase is reduced modulo `2*N` before applying `pi/N`, avoiding
unnecessarily large trigonometric arguments for ordinary representable inputs.

This is numerical floating-point arithmetic, not exact roots-of-unity
arithmetic.

## Size safety

For a non-power-of-two input of length `N`, the linear-convolution span is
`2*N-1`.

Production first verifies that this span is representable in `size_t`, then
forms it as

`N + (N-1)`

rather than an overflowing intermediate `2*N`.

The existing checked `next_power_of_two` helper rejects a padded FFT size that
cannot itself be represented.

## Independent verification

Committed tests use a direct quadratic DFT oracle that enumerates every
frequency/time pair and does not use FFT butterflies, chirp convolution, or the
production recurrence.

Coverage includes:

- empty and singleton behavior;
- non-finite rejection for both power-of-two and non-power-of-two lengths;
- fixed prime and composite lengths;
- power-of-two equality with the sealed radix-2 FFT;
- 360 fixed-seed random arbitrary lengths from 1 through 31 against direct DFT;
- both forward and inverse direct-DFT comparisons;
- 480 fixed-seed arbitrary-length forward/inverse round trips up to length 63.

A supplementary model-level sign/reduction check compared Bluestein convolution
with an independently enumerated direct DFT for 1,380 forward/inverse random
cases across lengths 1 through 23 and found zero mismatch. This is supporting
logic evidence only; repository compiler/sanitizer CI remains the integration
gate.

## Complexity boundary

Let `N` be the requested transform length and

`M = next_power_of_two(2*N-1)`.

For non-power-of-two lengths the implementation performs three radix-2 FFTs of
length `M` plus linear setup/finalization:

- arithmetic: `O(M log M)`;
- temporary storage: `O(M)`.

Power-of-two lengths retain the existing in-place radix-2 path.

## Non-claims

This slice does not claim:

- mixed-radix FFT;
- general chirp-z transforms with arbitrary contour parameters;
- exact integer/modular output;
- FFTW-level tuning;
- SIMD/vectorized kernels;
- cache or throughput benchmarks;
- rigorous universal floating-point error bounds;
- platform-bitwise reproducibility.

The implementation adds arbitrary transform length under the existing
floating-point FFT convention only.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/numerical/bluestein_fourier_transform.hpp`;
- `tests/test_bluestein_fourier_transform_cases.hpp`;
- `docs/scope_recovery_bluestein_fourier_transform.md`.

Pre-PR hardening fixed:

- a malformed hexadecimal test seed;
- explicit test inclusion of the exception contract header;
- convolution-span construction that could otherwise overflow in the
  intermediate `2*N` expression.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file changes are required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `1f896af9cc2edca47a112efba168bdd54ef0e225`.
