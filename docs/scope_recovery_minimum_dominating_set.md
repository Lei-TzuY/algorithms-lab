# Scope recovery: bounded exact minimum dominating set

## Recovery decision

This slice stays inside the post-Phase-69 algorithms/data-structures recovery
boundary. Fresh default-branch, PR-history, and branch searches find no minimum
dominating-set production capability. It deliberately moves away from the just-
merged Tutte deletion/contraction recurrence. Although directed feedback vertex
set is also a bounded exact NP-hard baseline, this solver uses a different
closed-neighborhood covering obligation and an admissible domination lower bound
rather than cycle-hitting search.

## Production contract

`minimum_dominating_set(graph)` accepts the repository undirected multigraph
`Graph` abstraction and returns a deterministic sorted minimum-cardinality
vertex set whose closed neighborhoods cover every vertex.

- directed input is rejected;
- parallel copies, self-loops, and stored edge weights do not change the
  unweighted domination relation;
- empty graph returns the empty witness;
- exact search is explicitly bounded to at most 24 vertices; larger inputs throw
  rather than hiding exponential work;
- diagnostics expose the number of distinct selected-subset states evaluated and
  the number of incumbent/lower-bound prunes.

The API does not claim lexicographically minimum optimum, weighted/directed/
connected domination, polynomial/FPT running time, kernelization, approximation,
or large-instance practicality.

## Exact search / proof obligation

For selected set `S`, the dominated set is exactly the union of `N[v]` over
`v in S`, so it is a pure function of the selected bitmask. Production memoizes
that mask and therefore expands each selected subset at most once.

At a non-terminal state, production chooses an undominated witness vertex `w`.
Every feasible extension must select at least one vertex of the closed
neighborhood `N[w]`, so branching once on every currently unselected candidate
in `N[w]` is exhaustive. Candidates are ordered by immediate new coverage and
then vertex id only as a deterministic search heuristic.

The incumbent starts from a deterministic greedy cover. For `R` currently
undominated vertices and maximum possible additional one-vertex gain `g`, every
completion needs at least `ceil(|R|/g)` further vertices. This is an admissible
lower bound because no one future selection can dominate more than `g` members
of `R`. States whose current size plus that bound cannot improve the incumbent
are safe to prune.

Together, exhaustive closed-neighborhood branching, safe memoization of selected
subsets, and admissible pruning preserve at least one optimum branch. Finite
tests are implementation evidence; they are not a proof that minimum dominating
set is tractable in general.

## Verification

Focused candidate execution passes repository-style strict GCC, strict Clang,
and actual GCC ASan+UBSan builds.

Deterministic coverage includes empty/singleton graphs, a star, a six-cycle,
parallel copies with distinct signed weights, a self-loop, directed rejection,
the exact 24-vertex boundary, and 25-vertex rejection.

The primary randomized oracle is structurally independent from branch-and-bound:
600 fixed-seed undirected multigraphs with `0..10` vertices enumerate every
vertex subset and directly replay the domination predicate. Production must
return exactly the exhaustive optimum cardinality, and every returned witness is
checked for uniqueness, sorted order, domination, repeat determinism, and the
`explored_states <= 2^V` memoized-state diagnostic.

## Complexity boundary

There are at most `2^V` selected masks. A direct state scans vertices and may
sort at most `V` branch candidates, giving the conservative code-local bound
`O(2^V * V log V + E)` time after neighborhood materialization and `O(2^V + V)`
auxiliary search state (excluding the graph itself and returned witness) under
the explicit `V <= 24` contract.
