# Scope recovery: exact Prüfer labeled-tree coding

## Coverage decision

A fresh live-state audit at `main@a5493962463f62247996e12d0720545a9d4d52e8`
after the merged Manacher checkpoint found no Prüfer/Prufer tree-code, Cayley-code,
or equivalent labeled-tree sequence codec in default-branch code, pull-request
history, or branch names. The authoritative post-Phase-69 recovery boundary remains
`docs/scope_recovery_after_phase69.md`; historical compiler/backend work stays
frozen.

This slice deliberately changes proof model again. The immediately preceding
checkpoints use palindrome mirror reuse and Burnside orbit averaging; this one
implements the classical bijection between labeled trees and length-`n-2`
sequences.

## Production contract

`prufer_encode(const Graph&)`:
- accepts a non-empty simple undirected tree on dense labels `[0,n)`;
- rejects directed graphs, the empty graph, self-loops, parallel edges,
  disconnected graphs, and cyclic/non-tree edge counts;
- ignores stored edge weights;
- repeatedly removes the smallest labeled leaf, producing the canonical Prüfer
  sequence for the fixed vertex labels.

`prufer_decode(n, code)`:
- requires `n >= 1` and exactly `max(n-2,0)` code entries;
- validates every encoded label before use;
- reconstructs the unique undirected labeled tree by repeatedly joining the
  smallest current leaf to the next code symbol;
- supports the singleton and two-vertex empty-code boundaries explicitly.

The direct educational implementation uses ordered adjacency sets and a min-heap,
so both directions claim `O(n log n)` time and `O(n)` auxiliary state. It does not
claim the known linear-time variants.

## Bijection / proof obligations

For encoding, a tree with at least two vertices always has a leaf. Removing a
leaf preserves a tree on the remaining labels; choosing the smallest leaf makes
the emitted sequence deterministic. A vertex appears in the sequence exactly
`degree(v)-1` times because every incident edge except the final one is removed
while that vertex is still the surviving neighbor.

For decoding, initialize `degree(v)=1+frequency(v)` and repeatedly connect the
smallest degree-one label to the next sequence symbol. The same degree identity
shows that each step reverses one canonical encoding deletion. Two leaves remain
at the end and form the final edge. Thus, for `n>=2`, labeled trees on `[0,n)` and
length-`n-2` sequences over `[0,n)` are mutually inverse.

The classical Prüfer/Cayley bijection is the mathematical proof obligation;
finite tests are implementation evidence rather than a substitute proof.

## Independent verification

Focused candidate passed:
- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection and fail-fast options: 4/4.

Committed evidence includes singleton/two-vertex boundaries, known sequences,
ignored signed weights, directed/self-loop/parallel/cycle rejection, and a
`V-1`-edge disconnected regression (triangle plus isolated vertex).

Two independent exhaustive checks avoid relying only on encode/decode round trips:

1. for every simple graph on `n<=5`, enumerate edge subsets, independently decide
   treehood by edge count plus BFS, and compare production encoding against a
   separate adjacency-matrix implementation that linearly scans for the smallest
   leaf at each step;
2. for every Prüfer sequence through `n=7` (all `n^(n-2)` sequences), decode,
   independently replay simple-tree validity, re-encode exactly, and verify
   `degree(v)=1+frequency(v)` for every vertex.

The observed counts of simple labeled trees for `n<=5` also equal `n^(n-2)`, but
that finite observation is not presented as a proof of Cayley's theorem.

## Scope / non-claims

Exactly four paths change:
- `include/algorithms/graphs/prufer_code.hpp`;
- `tests/test_prufer_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this recovery proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or unrelated recovery surface is modified. This slice
adds no random-tree sampler, unlabeled-tree canonicalization, tree-isomorphism
solver, arbitrary vertex labels, or enumeration API.
