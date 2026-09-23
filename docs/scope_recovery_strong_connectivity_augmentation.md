# Scope recovery: minimum strong-connectivity augmentation

## Coverage decision

After the safeguarded Brent solver merged as
`main@f7905981176c99c2a63c053bba80373661ef2f26`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains:

- exact strongly connected components and condensation DAG construction;
- low-link / biconnectivity analysis;
- Robbins strong orientation for orienting suitable undirected graphs;
- online bridge connectivity.

It did not contain a directed strong-connectivity **augmentation** algorithm,
branch, or implementation PR.

This slice therefore reuses the existing SCC primitive and adds a new
constructive optimization capability: return a minimum-cardinality set of
directed edges whose addition makes an arbitrary directed graph strongly
connected.

An already-populated `scope-recovery-rans-byte-coding` branch was explicitly
left untouched because that surface is occupied by parallel work.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; frozen historical compiler/backend work
is not resumed.

## Public contract

`minimum_strong_connectivity_augmentation(graph)`:

- accepts directed graphs only;
- returns original-vertex endpoint pairs to add;
- returns no edges for an empty graph;
- returns no edges when the graph already consists of one SCC;
- otherwise returns an exact minimum-cardinality augmentation;
- supports self-loops and parallel input edges because those are already valid
  `Graph` inputs;
- never relies on edge weights.

The returned edges connect distinct SCCs. Since every source endpoint is chosen
from a sink SCC of the condensation DAG, the returned edge cannot already be an
inter-SCC edge of the input.

This slice optimizes **edge count only**. It does not optimize weights, costs,
forbidden endpoints, degree bounds, or lexicographic output among all optimum
augmentations.

## Condensation lower bound

Contract the input graph into its SCC condensation DAG `D`.

If `D` has at most one vertex, no augmentation is required.

Otherwise let:

- `S` = number of source SCCs (zero indegree);
- `T` = number of sink SCCs (zero outdegree).

Every strong augmentation must add at least one incoming inter-SCC edge to each
source SCC, otherwise that source still cannot be reached from the rest of the
graph.

Likewise every sink SCC needs at least one new outgoing inter-SCC edge.

One added directed edge can satisfy at most one source-incoming obligation and
one sink-outgoing obligation, so every augmentation has size at least

`max(S,T)`.

The production construction attains that bound exactly.

## Reachability bipartite graph

Build a bipartite graph whose left side is the source SCCs and whose right side
is the sink SCCs.

A source `s` is adjacent to a sink `t` exactly when the condensation DAG
contains a directed path `s ->* t`.

Production computes this relation by a graph traversal from each source SCC,
then finds a deterministic maximum bipartite matching with augmenting-path DFS.

The matching is nonempty for every nontrivial condensation DAG because every
source reaches at least one sink.

## Matched core

Let the maximum matching contain pairs

`(s_0,t_0), ..., (s_{q-1},t_{q-1})`

where each matched source reaches its matched sink.

Production adds

`t_i -> s_(i+1 mod q)`

for every matched pair.

Therefore:

`s_i ->* t_i -> s_(i+1) ->* t_(i+1)`

forms one directed cycle through all matched source/sink endpoints.

All matched endpoints are consequently in one strongly connected core.

## Maximum-matching endpoint lemma

Consider an unmatched source SCC `u`.

It reaches at least one sink SCC. If it reached an **unmatched** sink, that
single reachability edge would augment the current matching, contradicting
maximum cardinality.

Therefore every sink reachable from an unmatched source is matched. In
particular, every unmatched source has a path into the matched core.

Dually, every unmatched sink is reachable from at least one source. If an
unmatched source could reach it, the matching would again admit an augmenting
edge.

Therefore every source that reaches an unmatched sink is matched. Every
unmatched sink is consequently reachable **from** the matched core.

## Attaching unmatched endpoints

Production pairs as many unmatched sinks and unmatched sources as possible and
adds

`unmatched_sink -> unmatched_source`.

For each such pair:

- the matched core already reaches the unmatched sink;
- the new edge reaches the unmatched source;
- the unmatched source already reaches a matched sink and hence the core.

Both endpoints join the strong core with one added edge.

If unmatched sources remain after all unmatched sinks are used, production adds
one edge from a fixed matched sink to each remaining source. Each such source
already has a path back into the matched core.

If unmatched sinks remain, production adds one edge from each remaining sink to
a fixed matched source. Each such sink is already reachable from the matched
core.

If the matching size is `q`, with

- `S-q` unmatched sources;
- `T-q` unmatched sinks,

the construction adds exactly

`q + max(S-q,T-q) = max(S,T)`

edges and therefore meets the lower bound.

## Why every SCC joins the result

Every vertex of a finite DAG lies on at least one path from a source to a sink:

- repeatedly following predecessors must terminate at a source;
- repeatedly following successors must terminate at a sink.

After the construction, every source and every sink SCC belongs to the same
strongly connected augmented core.

For any internal condensation vertex `v`, choose a source `s` and sink `t`
with

`s ->* v ->* t`.

The core can reach `s`, hence can reach `v`; and `v` reaches `t`, which
can reach the core.

Thus every condensation SCC, and therefore every original vertex, belongs to
one SCC after adding the returned edges.

## Determinism

For each SCC, production chooses the smallest original vertex as its concrete
representative.

Source and sink component lists are sorted by those representatives.

The reachability matching scans sources and sinks in that deterministic order,
so a fixed graph representation yields a deterministic augmentation witness.

Determinism is not an optimality tie-break claim among all possible minimum
augmentations.

## Complexity boundary

Let:

- `V,E` = original graph vertices/edges;
- `C,E_c` = condensation DAG vertices/edges;
- `S,T` = source/sink SCC counts.

Existing Tarjan SCC decomposition is `O(V+E)`.

The repository's deterministic `condensation_graph()` removes duplicate
inter-SCC edges with `std::set`, so its documented bound is `O(E log E)`.

Source-to-sink reachability costs

`O(S * (C + E_c))`.

The bipartite graph has at most `S*T` edges. The straightforward augmenting
DFS matching used here has conservative bound

`O(S * (S*T)) = O(S^2 T)`.

Construction after matching is linear in `S+T`.

A conservative combined bound is therefore:

`O(E log E + S(C+E_c) + S^2 T + V)`.

Peak auxiliary storage is conservatively

`O(V + E + S*T)`,

including SCC/condensation work and the explicit source-to-sink reachability
matrix.

No tighter asymptotic claim is made.

## Independent verification

Committed verification deliberately does not use Tarjan SCCs or the production
condensation proof path as its oracle.

A direct test helper checks strong connectivity by:

1. forward reachability from one original vertex;
2. reverse-graph reachability from the same vertex.

For tiny graphs, an independent exact oracle enumerates missing non-loop
directed edges by subset size. It returns the first cardinality for which some
added-edge subset makes the graph strongly connected.

Committed cases cover:

- empty graph;
- singleton graph;
- already strongly connected cycle;
- directed chain;
- two isolated vertices;
- source-heavy out-star;
- sink-heavy in-star;
- multiple nontrivial SCCs with original-vertex representatives;
- input self-loops;
- parallel edges;
- undirected input rejection;
- all 64 simple directed graphs on three labeled vertices;
- 180 fixed-seed random simple directed graphs on four vertices.

For every oracle-tested graph, tests require:

- returned endpoints are in range;
- no returned self-loop;
- no duplicate returned edge;
- no returned edge already exists in the input;
- applying every returned edge makes the graph strongly connected;
- returned cardinality equals the independent exhaustive optimum.

Supplementary model-level verification additionally ran the matching
construction over **all 4,096 simple directed graphs on four labeled vertices**.
Every result attained the source/sink lower bound and produced a strongly
connected augmented graph; zero counterexamples were found.

That model run is supporting evidence only. Repository compiler, release-test,
and sanitizer CI remain the integration gate.

## Non-claims

This slice does not claim:

- minimum-cost or weighted augmentation;
- forbidden-edge constraints;
- prescribed augmentation endpoints;
- degree-constrained augmentation;
- vertex-addition augmentation;
- dynamic/incremental maintenance;
- optimal augmentation under any metric other than edge count;
- asymptotically optimal source-to-sink matching implementation;
- benchmark-backed performance.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/graphs/strong_connectivity_augmentation.hpp`;
- `tests/test_strong_connectivity_augmentation_cases.hpp`;
- `docs/scope_recovery_strong_connectivity_augmentation.md`.

Pre-PR hardening includes:

- replacing macro-hostile `std::pair<A,B>` expressions inside test macros with
  named expected vectors;
- simplifying matched-cycle wiring to remove unused structured bindings;
- correcting time complexity to include condensation duplicate removal;
- correcting peak auxiliary-space accounting to include condensation storage.

No existing SCC implementation, CMake, test-main, README, historical ROADMAP,
workflow, benchmark, compiler/backend path, or temporary construction file is
changed.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `f7905981176c99c2a63c053bba80373661ef2f26`.
