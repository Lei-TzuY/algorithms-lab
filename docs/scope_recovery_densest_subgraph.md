# Scope recovery: exact densest induced subgraph

## Coverage decision

After constructive st-numbering reached merged `main`, a fresh code, commit,
pull-request, and branch audit deliberately left the recent two-vertex-connected
construction streak. The repository already contains directed minimum
arborescence, stable matching/roommates, minimum-mean cycle, van Emde Boas sets,
Fibonacci heaps, and many other obvious textbook frontiers, but no densest-
subgraph capability was found.

This slice therefore adds exact maximum-density non-empty induced subgraph on an
undirected loopless multigraph. Density is the number of logical internal edge
copies divided by the number of selected vertices. Parallel edges count with
multiplicity and stored edge weights are intentionally ignored. Directed input
and self-loops are rejected. Empty input returns the empty witness; a non-empty
edgeless graph returns the deterministic singleton `{0}`.

## Exact reduction

Let `n = |V|`, `m = |E|`, and `Q = n^2 + 1`. For an integer threshold numerator
`k`, the decision question is whether some non-empty `S` satisfies

`|E(S)| / |S| > k / Q`.

The implementation reuses the sealed Dinic max-flow capability. Build an s-t
network with capacities

- `s -> v : mQ`,
- `v -> t : mQ + 2k - deg(v)Q`,
- for each undirected logical edge `{u,v}`, both `u -> v` and `v -> u` have
  capacity `Q`.

For source-side vertex subset `S`, the cut is

`mnQ + 2(k|S| - Q|E(S)|)`.

Thus a cut below the baseline `mnQ` exists exactly when a subset has density
strictly above `k/Q`. Distinct feasible densities `e/v`, with denominator at
most `n`, differ by at least `1/n^2`. Since `1/Q < 1/n^2`, the final feasible
integer threshold isolates the true optimum: no strictly suboptimal feasible
density can remain above that threshold. The source side of the final minimum
cut is therefore an exact optimum witness.

A binary search over `k in [0,mQ]` uses `O(log(mQ))` max-flow decisions. All
scale, capacity, baseline, and reduction-size arithmetic is checked before use;
the function rejects instances whose exact integer reduction is not
representable by the repository's signed 64-bit `Capacity` type.

## Verification

The primary oracle is independent exhaustive enumeration of every non-empty
vertex subset for fixed-seed random multigraphs with at most eight vertices.
Each candidate density is compared exactly by cross multiplication. The returned
vertex witness is separately replayed against the original logical edge
multiplicity.

Focused evidence covers:

- empty and edgeless graphs,
- a dense component competing with a sparse component,
- parallel copies carrying different ignored weights,
- deterministic replay,
- directed/self-loop rejection,
- 400 fixed-seed random loopless undirected multigraphs against the exhaustive
  subset oracle,
- strict GCC and Clang warning gates, plus actual GCC ASan+UBSan execution.

The randomized corpus is evidence for the implementation, not a proof of the
reduction; exactness follows from the cut identity and rational-isolation
argument above.

## Non-claims

This is not weighted densest subgraph, directed density optimization, a self-loop
extension, a parametric-flow implementation, or a strongly-polynomial runtime
claim. The direct implementation intentionally reuses the existing max-flow
engine rather than duplicating residual-network machinery.
