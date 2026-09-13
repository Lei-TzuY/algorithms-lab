# Scope recovery: constructive st-numbering

## Coverage decision

A fresh live audit after the constructive open-ear checkpoint found no
`st_numbering` / bipolar-ordering capability in default-branch code, PR history,
or the branch namespace. The historical compiler/backend ROADMAP remains frozen;
this slice follows the post-Phase-69 scope-recovery authority instead.

The new capability deliberately consumes the just-sealed constructive open-ear
witness rather than duplicating low-link or biconnected recognition. It turns a
two-vertex-connectivity certificate into an ordering that can be consumed by
bipolar-orientation and planar/graph-layout algorithms.

## Production contract

`st_numbering(graph)` accepts the same classical simple-graph domain as
`open_ear_decomposition`:

- input must be undirected;
- self-loops and parallel structural edges are rejected;
- edge weights are ignored;
- fewer-than-three, disconnected, or articulation-bearing graphs return
  `not_two_vertex_connected`;
- an articulation witness is propagated when the sealed open-ear gate exposes
  one.

On success the result contains a permutation `order` and its inverse `rank`.
`order.front()` is `s`, `order.back()` is `t`, `s-t` is a real graph edge, and
every other vertex has at least one lower-ranked and at least one higher-ranked
neighbor.

The construction is deterministic because the reused open-ear witness is
deterministic.

## Construction and invariant

The initial ear is a cycle whose closing edge is `(s,t)`. Removing that closing
edge gives the initial order `s, ..., t`; every internal cycle vertex already has
a predecessor and successor neighbor.

For each later nontrivial ear `u, x1, ..., xk, v`, compare the current ranks of
its old endpoints. Insert the new internal vertices immediately after the earlier
endpoint, using the ear direction if `u` is earlier and reversing the internal
sequence if `v` is earlier. Existing vertices preserve their relative order and
therefore keep any lower/higher witnesses they already had. Every inserted
internal vertex obtains a lower neighbor and a higher neighbor from the ear path.
Single-edge ears add no vertices and require no ordering change.

Induction over the ear sequence therefore yields the st-numbering invariant.
Orienting every edge from lower rank to higher rank gives an acyclic bipolar
orientation with unique source `s` and unique sink `t`; this PR returns the
ordering/rank witness rather than introducing a second orientation API.

## Verification

Focused pre-upload checks passed under strict GCC and Clang syntax/warning gates.
The ear-to-order conversion was additionally executed under GCC release, Clang
release, and GCC ASan+UBSan on 5,000 fixed-seed randomly generated valid ear
sequences, checking permutation/rank consistency, the closing `s-t` edge, and the
lower/higher-neighbor property for every internal vertex.

Repository tests add an independent primary existence oracle: for fixed-seed
simple undirected graphs with up to eight vertices, delete each vertex and run BFS
on the remainder to decide two-vertex connectivity. Production success must
match that oracle. Successful st-numbering witnesses are replayed directly on the
original graph; failures with an articulation witness are checked by deleting
that vertex and proving disconnection.

The randomized graph generator consumes raw `std::mt19937_64` output directly
rather than relying on implementation-specific distribution mappings.

## Complexity and non-claims

The direct vector-insertion conversion costs `O(V^2)` time and `O(V)` additional
storage. Including the reused open-ear construction, the conservative end-to-end
bound remains `O(V(V+E))` time and `O(V+E)` storage.

No linear-time st-numbering algorithm, multigraph st-numbering, dynamic update,
planarity embedding, or canonical minimum/lexicographic st-numbering claim is
made. Tests are implementation evidence; the ear-extension argument above is the
proof obligation for the ordering property.
