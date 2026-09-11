# Scope recovery: explicit-family Count-Min frequency sketch

## Coverage decision
A fresh recovery audit finds no Count-Min sketch implementation. The sealed Misra-Gries phase is deterministic and its own sealing audit explicitly reserves probabilistic sketches for a separate hash-family probability contract. This slice therefore changes proof model instead of adding another deterministic heavy-hitter wrapper.

## Production contract
`CountMinSketch` stores non-negative `uint64_t` frequency updates for `uint32_t` keys. The caller supplies every hash row explicitly as affine coefficients `(a,b)` over the prime field `P = 4294967311`, with `1 <= a < P` and `0 <= b < P`. Since `P > 2^32`, every key is embedded injectively in the field. A row maps

`x -> ((a*x + b mod P) mod width)`.

The sketch returns the minimum row counter. Consequently it never underestimates for any valid explicit rows, regardless of how those rows were selected. Updates are preflighted across total weight and every touched counter before mutation, so integer overflow is rejected transactionally.

## Probability contract
The probabilistic guarantee is conditional and explicit: it applies only when each row's `(a,b)` is sampled independently and uniformly from `{1,...,P-1} x {0,...,P-1}`. Supplying deterministic, correlated, adversarial, or replay-seeded coefficients does not inherit this theorem merely because the same data structure is used.

For distinct field keys `x != y`, the affine map from `(a,b)` to `(ax+b, ay+b)` is a bijection onto ordered pairs of distinct field elements. Let `P = s*w + r` for sketch width `w`. Reduction modulo `w` partitions the field into `r` buckets of size `s+1` and `w-r` buckets of size `s`. Therefore the exact per-row collision probability is

`q_w = [r(s+1)s + (w-r)s(s-1)] / [P(P-1)]`.

For stream total weight `N` and true frequency `f_x`, one row's expected collision overcount is `q_w (N-f_x) <= q_w N`. Markov gives

`Pr[row overcount >= 2 q_w N] <= 1/2`.

With `d` independently sampled rows, the Count-Min estimate exceeds `f_x + 2 q_w N` only if every row has that much overcount, hence

`Pr[estimate(x) - f_x >= 2 q_w N] <= 2^-d`.

This is the probability claim. The implementation does not pretend that an arbitrary deterministic seed proves independent uniform row selection, does not claim cryptographic hashing, and does not convert empirical collision rates into a theorem.

## Complexity and representation
With width `w` and depth `d`, state is `O(wd)`. Construction is `O(wd)` zero initialization. Each row hash reuses the repository's overflow-safe modular multiplication, whose direct implementation is logarithmic in its second operand; therefore update and estimate are conservatively `O(d log U)` for key universe `U`, with at most 32 multiplication/doubling rounds for the fixed `uint32_t` domain. Under the conventional fixed-word model this is `O(d)`. The expensive `valid_state()` diagnostic scans all counters and is `O(wd)`. The implementation supports widths in `[1,P]` and at least one row.

## Verification
Deterministic tests cover validation, zero updates, exact no-underestimate behavior, transactional `uint64_t` overflow, and state invariants. Five hundred fixed-seed streams compare every queried key against an exact frequency map, requiring `true <= estimate <= total`.

The hash-family counting argument is also exercised independently on the complete affine family of the toy prime `17`: for several widths and every distinct key pair, exhaustive `(a,b)` enumeration must equal the closed-form same-bucket count above. This bounded executable check supports the algebraic proof shape; it is not used to infer a probability guarantee for arbitrary caller-chosen production rows.
