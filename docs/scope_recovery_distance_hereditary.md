# Scope recovery: distance-hereditary graph pruning

## Coverage decision

A fresh live-state audit after the persistent Hood-Melville real-time queue
checkpoint merged as
`main@b410297faf9107d935b0312e8efe56910cd64e9d` found zero open pull
requests and zero open issues. Default-branch code, branch names, commit search,
and pull-request history contained no distance-hereditary recognizer or pruning
decomposition.

The repository already contains cograph/cotree recognition, chordal recognition,
low-link/block-cut decomposition, cactus decomposition, and several dynamic graph
indices. This slice changes proof model again: it recognizes a hereditary graph
class through a replayable pendant/twin elimination sequence and verifies that
result against the defining induced-distance property rather than another
decomposition recurrence.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical ROADMAP are untouched.

## Production contract

`distance_hereditary_pruning(graph)` accepts an undirected `Graph` and either
returns a deterministic pruning witness or `nullopt`.

The structural simple graph is used:

- self-loops are ignored;
- parallel copies collapse;
- stored edge weights do not affect recognition;
- directed input is rejected.

For a nonempty recognized graph the witness contains exactly `V-1` removal
steps plus one final survivor. Each removal step is one of:

- **pendant** — the removed vertex has exactly one active neighbor, recorded as
  its reference vertex;
- **false twin** — the removed vertex and reference are nonadjacent and have
  identical neighborhoods outside the pair;
- **true twin** — the removed vertex and reference are adjacent and have
  identical neighborhoods outside the pair.

The empty graph returns an empty witness with no survivor.

`valid_distance_hereditary_pruning` replays a supplied witness against the
original structural simple graph and checks every active-set relation directly.

## Recognition theorem / proof obligation

A finite graph is distance-hereditary iff every induced subgraph with at least
two vertices contains either a pendant vertex or a pair of twins. Equivalently,
the graph can be reduced to one vertex by repeatedly deleting a pendant vertex,
a false twin, or a true twin.

Production uses this characterization directly.

At every iteration it first chooses the smallest active pendant vertex if one
exists. Otherwise it chooses the lexicographically first active twin pair. If
neither operation exists while more than one vertex remains, the graph is
rejected.

Deleting one valid pendant/twin vertex preserves the distance-hereditary class;
repeating until one survivor therefore produces a replayable constructive
certificate.

The implementation deliberately favors simple deterministic witness generation
over an optimized linear-time recognition algorithm.

## Independent definition oracle

The primary committed test oracle does **not** reuse the pendant/twin
characterization.

For a graph on at most ten test vertices it enumerates every connected induced
subgraph and checks the defining metric property:

> every pairwise shortest-path distance inside the connected induced subgraph is
> exactly the same as that pair's distance in the original graph.

The oracle computes restricted BFS distances independently from the production
pruning algorithm.

Before repository upload, the same characterization was separately model-checked
against this induced-distance definition for every simple graph on
`0..6` vertices: 33,868 graphs total, with no mismatch. That model check is
supporting evidence only; exact repository C++ CI remains mandatory.

## Deterministic and randomized evidence

Committed tests include:

- empty and single-vertex boundaries;
- explicit directed-input rejection;
- disconnected distance-hereditary structure;
- pendant and twin elimination behavior;
- rejection of an induced five-cycle;
- invariance under parallel edges, self-loops, and arbitrary stored weights;
- exhaustive comparison against the independent induced-distance definition for
  every simple graph on `0..5` vertices;
- 500 fixed-seed random multigraphs on `0..7` vertices, including parallel
  edges, self-loops, and arbitrary weights, again checked against the
  definition oracle;
- replay validation of every returned pruning witness.

The definition oracle does not inspect production active sets, twin comparisons,
step ordering, or witness construction.

## Complexity / non-claims

The production implementation uses a dense `V x V` simple adjacency matrix and
straightforward active-set scans.

With `V` vertices:

- resident storage: `O(V^2)`;
- one pendant scan: `O(V^2)`;
- one full twin search: `O(V^3)`;
- at most `V-1` pruning rounds;
- conservative total worst-case recognition time: `O(V^4)`;
- witness replay validation has the same conservative worst-case order.

This recovery slice makes no linear-time recognition claim, no sparse-optimality
claim, no dynamic-update claim, no canonical-isomorphism claim, and no
benchmark-backed speed claim. The classical faster distance-hereditary
recognizers remain outside this scope.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/graphs/distance_hereditary.hpp`;
- `tests/test_distance_hereditary_cases.hpp`;
- `docs/scope_recovery_distance_hereditary.md`.

One pre-PR test-fix commit corrects a `size_t` underflow in the exhaustive
`n=0` graph-count setup; it does not expand scope.

The isolated recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. No CMake, test-main, README, historical ROADMAP, workflow,
benchmark, frozen compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `b410297faf9107d935b0312e8efe56910cd64e9d`.
