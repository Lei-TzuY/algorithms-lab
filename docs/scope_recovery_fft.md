# Scope recovery: radix-2 complex fast Fourier transform

## Coverage decision

Fresh live-state, default-branch code, pull-request-history, and branch searches at
`main@70bb66590ecb32bfa15d9965de1423a1ffe31f63` found no floating-point FFT,
discrete Fourier transform, or equivalent complex transform implementation, no
historical FFT pull request, and no occupied FFT branch. The latest merged recovery
slice is exact parity-game winning regions. The long-lived minimum-cycle-basis
surface remains intentionally untouched.

This slice deliberately changes proof and arithmetic model instead of extending
recent graph/game/decomposition work. The repository already contains exact
modular NTT/CRT polynomial convolution; floating-point FFT adds a distinct
numerical contract with roundoff rather than pretending to be another exact
polynomial transform.

## Production contract

`fast_fourier_transform(values, inverse)` is a first-principles in-place radix-2
Cooley-Tukey transform over `std::complex<double>`:

- empty input is a no-op;
- non-empty length must be a power of two;
- every input component must be finite;
- the forward transform uses `exp(-2*pi*i*k/n)`;
- the inverse uses the opposite sign and divides by `n`;
- bit reversal and iterative butterfly stages are implemented directly.

`fft_real_convolution(left, right)` zero-pads two finite real sequences to the next
power of two, transforms both, multiplies pointwise, applies the inverse transform,
and returns the requested real prefix. Empty input yields an empty result.

The public names and documentation deliberately say FFT/floating-point convolution,
not exact convolution. Callers requiring exact modular/integer polynomial output
must use the sealed NTT/CRT surfaces.

## Algorithm / proof boundary

The radix-2 recurrence splits the DFT into its even-index and odd-index halves and
combines them with stage roots of unity. Iterative bit reversal places inputs in
the order needed by the bottom-up butterflies. At a stage of length `L`, each
butterfly combines two already-correct length-`L/2` subtransforms with successive
powers of the primitive complex `L`th root. The inverse applies conjugate-sign
roots and the global `1/n` normalization.

Those algebraic identities establish the transform structure. This implementation
uses IEEE-754 `double`, so arithmetic is rounded: tests provide numerical evidence
under explicit tolerances and do not turn floating output into an exactness claim.
No universal forward-error bound is claimed beyond the standard floating-point
model of the operations actually executed.

## Independent verification

Focused exact-candidate verification passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

The verification surface includes:

- empty/singleton behavior, non-power-of-two rejection, and non-finite rejection;
- direct `O(n^2)` DFT comparison for 480 fixed-seed random complex vectors across
  transform sizes 1, 2, 4, 8, 16, and 32;
- forward-then-inverse round trips for 720 fixed-seed random complex vectors up to
  size 256;
- a deterministic signed convolution example plus 500 fixed-seed random real
  convolutions against an independent quadratic convolution oracle.

The direct DFT oracle enumerates every `(frequency,time)` term and contains no bit
reversal or Cooley-Tukey recurrence. The convolution oracle enumerates every input
coefficient pair. Tolerances scale with problem magnitude and expected output; they
are evidence for this implementation, not theorem-derived universal error bounds.

## Complexity / non-claims

For `n` transform values, the iterative transform uses `O(n log n)` complex
arithmetic and `O(1)` auxiliary storage beyond the caller-owned array. Real
convolution of lengths `a` and `b` uses a padded transform length
`N = next_power_of_two(a+b-1)`, `O(N log N)` arithmetic, and `O(N)` temporary
storage.

No exact-integer rounding guarantee, arbitrary-length Bluestein transform,
mixed-radix transform, FFTW-level tuning, SIMD/vectorization claim, benchmark-backed
speedup, platform-bitwise reproducibility, or rigorous global floating-error bound
is implied.

## Scope

Exactly four paths differ from the live base:

- `include/algorithms/numerical/fast_fourier_transform.hpp`;
- `tests/test_fft_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_fft.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
minimum-cycle-basis, frozen compiler/backend, or temporary-file surface changes.
