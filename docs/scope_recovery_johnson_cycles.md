# Scope recovery: Johnson elementary directed-cycle enumeration

## Coverage decision

A fresh live-state audit at `main@1031a07d3f489130dd51bd2586926e9d01ed1e92`
found no elementary/simple-cycle enumeration API, no Johnson-cycle PR, and no
occupied cycle-enumeration branch. The repository already contains Hamiltonian
cycle, minimum-cycle-basis, minimum-mean-cycle, SCC, and ordinary cycle-detection
capabilities; none enumerates every simple directed cycle. Held-Karp TSP,
Fibonacci heap, and exact Steiner-tree candidates were explicitly rejected during
this audit because equivalent capabilities already exist. The frozen Phase-45--69
compiler/backend roadmap remains out of scope under
`docs/scope_recovery_after_phase69.md`.

This slice deliberately changes proof model after exact CYK and Ukkonen recovery:
it is output-sensitive directed-graph enumeration with SCC restriction and a
blocked/unblock dependency invariant, not another string index or parser.

## Production contract

`johnson_elementary_cycles(graph)` accepts a directed `Graph` and returns every
distinct simple directed vertex cycle exactly once.

- undirected input is rejected;
- edge weights are ignored;
- parallel arc copies collapse to one structural adjacency relation, so they do
  not duplicate a vertex cycle;
- a self-loop is the one-vertex cycle `{v}`;
- no cycle repeats a vertex;
- every cycle begins at its minimum-numbered vertex;
- the complete result is deterministic and lexicographically sorted.

The output intentionally represents vertex cycles rather than logical edge-copy
cycles. That makes parallel-edge multiplicity a documented structural non-feature,
not an accidental duplicate-suppression behavior.

## Johnson invariant

Production first canonicalizes each outgoing adjacency list. For a monotonically
increasing lower vertex bound `s`, it builds the induced suffix graph on vertices
`>= s`, computes SCCs, and selects the cyclic SCC with the smallest vertex.
Within that SCC it runs Johnson's circuit procedure from the selected minimum
vertex.

`blocked[v]` means the current search has not yet established a path from `v`
back to the chosen start under the current DFS context. `B[w]` stores vertices
whose failed searches depend on `w`; when a cycle is found, recursive `unblock`
clears exactly the dependency closure that may now reach the start. Failed
vertices are left blocked and registered in successor dependency lists, avoiding
re-exploration that cannot produce a new cycle in the current SCC search.

After exhausting the chosen start, the lower bound advances past it. Therefore a
cycle is considered only when its minimum vertex is the active start, preventing
rotational duplicates. Canonical structural adjacency prevents duplicates caused
by parallel arc copies.

Correctness relies on Johnson's elementary-circuit enumeration theorem plus the
SCC fact that every directed cycle lies wholly inside one strongly connected
component. These are proof obligations; finite tests are implementation evidence,
not theorem substitutes.

## Independent verification

The primary oracle deliberately does not use SCC decomposition, blocked sets, or
Johnson's recurrence. For each possible minimum start vertex it performs a direct
simple-path DFS, forbids vertices smaller than the start, and records a cycle
whenever an outgoing arc returns to the start. Canonicalized adjacency is used
only to match the public structural parallel-edge semantics.

Committed evidence covers:

- empty directed input and undirected rejection;
- self-loops, parallel arcs, ignored positive/negative weights, and overlapping
  cycles;
- exact deterministic repeatability and replay of every returned edge;
- 900 fixed-seed directed multigraphs with `0..7` vertices and up to 24 random
  arc copies plus duplicate noise;
- exact vector equality between production and the independent exhaustive DFS
  oracle on every randomized graph.

The final focused candidate passed GCC C++20 strict warnings-as-errors, Clang
C++20 strict warnings-as-errors, and an actual GCC ASan+UBSan build, all 4/4.
Full-repository Actions on the uploaded exact candidate remain the integration
authority.

## Complexity and non-claims

Let `M` be input arc copies, `E` the number of unique structural arcs, and `C` the
number of returned cycles. Adjacency canonicalization costs at most
`O(M log M)`. The Johnson SCC/blocked-set core has the classical
`O((V+E)(C+1))` output-sensitive bound. The public deterministic final sort adds
`O(C log C * V)` worst-case cycle-comparison work. Auxiliary working state is
`O(V+E)` excluding returned cycles and recursion stack.

This slice does not claim edge-distinct parallel-arc cycles, undirected cycle
enumeration, chordless-cycle enumeration, minimum cycle basis, Hamiltonian-only
search, iterative/process-stack-independent traversal, dynamic updates, or a
stronger output-sorting bound.

## Scope

Exactly five files change: CMake source/test registration, one public API header,
one production source, one repo-native test translation unit, and this proof
boundary. README, ROADMAP, recovery authority, workflow, benchmark, frozen
compiler/backend surfaces, and unrelated recovery work remain untouched.
