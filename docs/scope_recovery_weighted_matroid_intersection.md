# Scope recovery: weighted matroid intersection

## Coverage decision

The live repository already contains an exact maximum-cardinality matroid-intersection baseline over two black-box independence oracles. Its documented boundary explicitly excludes weighted matroid intersection. A fresh code/PR/branch audit at `main@1b4b6472` found no weighted implementation or occupied weighted-matroid recovery branch, while several superficially attractive recovery surfaces remain occupied by other branches. This slice therefore deepens an existing combinatorial abstraction rather than duplicating another data structure.

## Production contract

`maximum_weight_matroid_intersection(weights, first, second)` uses the existing `MatroidIndependenceOracle` contract over ground elements `[0,n)`, where `n == weights.size()`.

The objective is lexicographic:

1. maximize common-independent-set cardinality;
2. among all maximum-cardinality sets, maximize exact signed total weight.

This cardinality-first definition is intentional. Negative weights never justify returning a smaller common independent set. With unit weights, the cardinality objective collapses to the sealed unweighted problem.

Both oracles must accept the empty set and must describe genuine matroids. As with the unweighted solver, a finite black-box call schedule cannot certify hereditary/exchange axioms; those axioms remain an explicit caller precondition.

## Weighted exchange invariant

For the current common independent set `I`, production builds the same exchange graph orientation as the unweighted solver:

- outside `x` is a source when `I+x` is independent in the first matroid;
- outside `x` is a sink when `I+x` is independent in the second;
- `outside x -> inside y` exists when `I-y+x` is independent in the second matroid;
- `inside y -> outside x` exists when `I-y+x` is independent in the first matroid.

Give an outside element node cost `-w(x)` and an inside element node cost `+w(y)`. For any alternating augmenting path `P`, the path cost is exactly the negative of the weight change caused by toggling membership along `P`.

The invariant is stronger than in the unweighted algorithm: before each augmentation, `I` is maximum-weight among all common independent sets of cardinality `|I|`. Production uses Bellman-Ford over the explicit exchange graph to choose minimum path cost; equal-cost candidates are resolved by minimum hop count before deterministic id tie-breaking. The classical weighted matroid-intersection augmenting-path theorem then makes the toggled set maximum-weight at cardinality `|I|+1`. The minimum-hop tie is part of the proof boundary, not a cosmetic choice.

A reachable lexicographically improving cycle would contradict current-cardinality weight optimality. Production checks for such a post-relaxation improvement and fails closed with `std::logic_error` rather than continuing under a violated matroid/optimality invariant.

When no source-to-sink augmenting path exists, the ordinary matroid-intersection theorem certifies maximum cardinality, so the maintained set is the requested lexicographic optimum.

## Exact arithmetic boundary

Weights are signed `int64_t`. Before any oracle search, production requires

`sum(abs(weight[e])) <= INT64_MAX`.

`INT64_MIN` therefore lies outside the accepted domain. Under this precondition, every subset total and every simple alternating-path cost is exactly representable in `int64_t`. Checked addition/negation still fail closed on any violated internal assumption. The slice makes no arbitrary-precision or saturating-arithmetic claim.

## Verification

Focused repo-style verification passes under strict GCC, strict Clang, and actual GCC ASan+UBSan.

Deterministic cases cover:

- a weighted exchange where the individually heaviest element must be removed to reach the cardinality-two optimum;
- all-negative weights, proving cardinality takes precedence over raw weight;
- missing/invalid oracle contracts;
- `INT64_MIN` and aggregate-absolute-weight overflow rejection;
- deterministic replay.

The primary oracle is independent exhaustive subset enumeration. It checks the lexicographic `(cardinality, total weight)` optimum directly rather than reproducing exchange-graph shortest paths. The randomized corpus contains 700 partition/partition pairs and 400 graphic/partition instances with bounded ground sets and signed weights. Every production witness is replayed through both independence oracles.

Testing is implementation evidence, not a proof of the weighted augmenting-path theorem or of caller-supplied matroid axioms.

## Complexity and non-claims

For ground size `n`, final common rank `r`, and independence-oracle cost `T`, each augmentation materializes `O(n^2)` candidate exchanges. Explicit subset construction contributes `O(n^3)` non-oracle work and the oracle schedule contributes `O(n^2 T)`. Bellman-Ford on the `O(n)`-vertex, `O(n^2)`-arc exchange graph is also `O(n^3)`. The direct educational baseline therefore documents

`O(r n^3 + r n^2 T)` time and `O(n^2)` explicit algorithm storage, excluding oracle internals and temporary candidate vectors.

This slice does not claim reduced-cost Dijkstra/potential acceleration, rank/circuit-oracle acceleration, matroid parity, arbitrary-precision weights, weighted common-independent-set objectives that sacrifice cardinality, or theorem correctness from randomized testing. The frozen Phase-45–69 compiler/backend surface remains untouched.
