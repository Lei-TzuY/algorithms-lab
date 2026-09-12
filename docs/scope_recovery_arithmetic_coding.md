# Scope recovery: static arithmetic byte coding

## Coverage decision

After exact LZ78 dictionary parsing reached merged `main`, a fresh live code,
pull-request-history, and branch audit found no arithmetic coder, range coder, or
equivalent interval-renormalization capability in `algorithms-lab`. LZ77 and LZW
are also absent, but immediately extending the just-merged dictionary-coding
surface would add adjacent breadth. Static arithmetic coding instead introduces
a different proof model: cumulative probability intervals, finite-precision
renormalization, and deferred underflow bits.

The historical compiler/backend ROADMAP remains frozen by
`docs/scope_recovery_after_phase69.md`; this is another bounded recovery slice,
not a return to that subsystem.

## Public contract

`arithmetic_encode_bytes(input, frequencies)` and
`arithmetic_decode_bytes(stream, frequencies)` share an explicit static model of
256 non-negative byte frequencies.

- The total model frequency must not exceed `2^20`.
- Every encoded input byte must have positive model frequency.
- Bytes are interpreted through `unsigned char`, so `0x00`, `0x80`, and `0xff`
  have ordinary byte semantics independent of signed `char`.
- The returned stream stores logical bits as bytes `0` or `1` plus the exact
  decoded symbol count. The static frequency table is supplied out of band.
- Empty input produces an empty bit stream and zero symbol count.
- Decoder bit values other than zero/one are rejected. A zero-symbol stream may
  not carry bits.
- Arithmetic decoding uses the conventional implicit zero extension after the
  finite emitted prefix. This is decoding state completion, not an integrity or
  truncation-detection mechanism.

The representation is educational and replayable; it is not a packed or
self-describing file format.

## Finite-precision interval invariant

Production uses the classical 32-bit inclusive arithmetic-coding interval
`[low, high]`, initially `[0, 2^32-1]`. For a symbol with cumulative range
`[c_lo,c_hi)` and total `T`, it applies

`high' = low + floor((high-low+1) * c_hi / T) - 1`

`low'  = low + floor((high-low+1) * c_lo / T)`.

All products are evaluated in `uint64_t`. The public `T <= 2^20` bound keeps
`range * cumulative < 2^52`, far below `uint64_t` overflow.

After every symbol the encoder repeatedly applies the standard E1/E2/E3 cases:

- lower-half containment emits `0`;
- upper-half containment emits `1` after subtracting one half;
- middle-half containment records one deferred underflow bit after subtracting
  one quarter;
- every renormalization then doubles the interval.

At a renormalization fixed point the interval straddles one half and is not
strictly contained inside the middle half. Hence either `low < 2^30` or
`high >= 3*2^30`, so the surviving interval width is greater than `2^30`.
Against total frequency at most `2^20`, every positive-frequency symbol retains
ample integer-state width before the next renormalization.

The decoder initializes a 32-bit code value from the emitted prefix plus implicit
zero extension. For a valid encoder stream it maintains `low <= value <= high`,
maps the scaled code point back to the unique positive-frequency cumulative
bucket, and mirrors the same E1/E2/E3 state transformation. The explicit symbol
count terminates decoding without an EOF pseudo-symbol.

## Independent evidence

The strongest small-instance oracle does not run another finite-precision coder.
For binary model frequencies `1:2`, it updates the ideal source interval as an
exact rational with a small `uint64_t` numerator/denominator. For 1,000 fixed-seed
sequences of length at most six, the complete dyadic interval represented by the
emitted bit prefix must lie inside that ideal arithmetic source interval. Bounds
are deliberately small enough that all oracle cross-products are independently
proven to fit `uint64_t`.

Additional committed evidence covers:

- a fixed equal-frequency `ABBA` bitstream vector;
- arbitrary `0x00`, `0x80`, and `0xff` bytes;
- zero/oversize models, zero-frequency input symbols, malformed logical bits,
  and malformed zero-symbol streams;
- 2,500 fixed-seed arbitrary-byte round trips under a uniform 256-byte model;
- 1,500 fixed-seed sparse/skewed static models with deterministic re-encoding;
- the exact maximum-total-frequency boundary with a one-symbol model.

Focused strict GCC, strict Clang, and actual GCC ASan+UBSan all pass the five
repo-equivalent tests before upload.

## Complexity and non-claims

Model construction is `O(256)`. For `n` source bytes and `b` emitted logical bits,
encoding is `O(n + b)` after fixed-alphabet cumulative construction; decoder
symbol lookup currently uses `std::upper_bound` over 257 cumulative boundaries,
so it is `O(n log 256 + b)`, effectively linear for the fixed byte alphabet.
Resident model/state is `O(256)` plus the returned bitstream/output.

This slice does **not** claim adaptive modeling, entropy bounds, compression-ratio
optimality, byte/word bit packing, self-describing serialization, corruption or
truncation detection, cryptographic properties, range-coder equivalence, or
benchmark-backed speed. It also intentionally does not add LZ77/LZW merely
because those neighboring dictionary methods remain absent.
