# Scope recovery: directed Matrix-Tree arborescence counting

## Fresh live coverage decision

This recovery slice follows `docs/scope_recovery_after_phase69.md`, not the frozen historical Phase-45--69 compiler/backend tail in `ROADMAP.md`.

The exact audited base is `main@a629830633a9b06e6ed7af3cd9512587c9078b93`, where optimal length-limited Huffman coding is merged and the exact push CI run `34967412669` completed successfully. Immediately before branch promotion, the repository had no open pull request.

Live code and pull-request history contain two nearby but distinct capabilities:

- sealed undirected Kirchhoff spanning-tree counting (`spanning_tree_count_mod_prime`), whose own proof boundary explicitly excludes directed arborescence counting;
- sealed directed minimum-cost rooted arborescence via Chu-Liu-Edmonds, which optimizes one witness rather than counting all witnesses.

Searches found no rooted directed arborescence-counting implementation or prior recovery PR. This slice therefore fills a real graph/finite-field gap rather than adding another coding variant after the length-limited Huffman checkpoint.

## Production contract

`rooted_out_arborescence_count_mod_prime(graph, root, modulus)` counts spanning **out-arborescences directed away from `root`** modulo a prime:

- input graph must be directed;
- `root` must be a valid vertex;
- every non-root vertex has exactly one selected incoming arc;
- following selected parent arcs from every non-root vertex must reach `root`;
- parallel arc copies preserve multiplicity;
- self-loops are ignored;
- stored edge weights are intentionally ignored;
- modulus must be prime and is validated with the repository's deterministic full-`uint64_t` primality routine;
- a singleton directed graph has count `1 mod p`.

No separate in-arborescence API is implied; callers can reverse arcs explicitly when they need the opposite orientation.

## Directed Matrix-Tree invariant

Let `A[u,v]` be directed arc multiplicity and let `D_in[v,v]` be the indegree multiplicity of `v`, excluding self-loops. Production forms

`L = D_in - A^T`.

Deleting the row and column of `root` yields a cofactor whose determinant is the number of rooted out-arborescences modulo `p` by the directed Matrix-Tree theorem.

The orientation is intentional. An arc `root -> v` contributes only to the surviving diagonal after the root column is deleted, while an arc `u -> root` lies in the deleted root row and cannot participate in an out-arborescence rooted at `root`.

The determinant is evaluated by first-principles dense Gaussian elimination over `F_p`. Multiplication and inversion reuse the repository's overflow-safe `multiply_mod` and `power_mod`; no unchecked `uint64_t` product is introduced.

Correctness relies on the directed Matrix-Tree theorem. Finite tests are implementation evidence, not a proof of that theorem.

## Independent verification

Focused exact candidate passed:

- GCC C++20 with repository strict warnings-as-errors: 6/6;
- Clang C++20 with repository strict warnings-as-errors: 6/6;
- actual GCC ASan+UBSan with fail-fast/leak detection: 6/6.

Deterministic evidence covers:

- directed/undirected contract validation, root bounds, and prime-modulus validation;
- singleton/self-loop behavior;
- a directed cycle and an asymmetric root-reachability case that would fail if the Laplacian orientation were reversed;
- parallel-arc multiplicity, ignored signed weights, self-loops, and arcs entering the root;
- complete directed graphs, including `K5` with rooted count `5^(5-2)=125`;
- the full-width prime `18446744073709551557`.

The primary randomized oracle is structurally independent of determinants. For 700 fixed-seed directed multigraphs with 1--6 vertices and up to 12 arc copies, it enumerates one concrete incoming arc copy for every non-root vertex. A choice is accepted exactly when every parent chain reaches the root; accepted assignments are counted modulo the same prime. Parallel copies therefore remain distinct oracle choices exactly as required by the public contract.

As secondary cross-layer integration evidence only, 300 fixed-seed undirected multigraphs are converted to bidirected graphs. For every such graph and random root, production directed count must equal the sealed undirected Kirchhoff spanning-tree count. This check is not the primary oracle because both implementations ultimately use Matrix-Tree determinants.

## Complexity and non-claims

For `V` vertices and `E` stored directed arcs, Laplacian construction is `O(E)`. Dense elimination uses `O(V^3)` field-operation slots. With the repository's repeated-doubling modular multiplication, the conservative direct bit-cost claim is `O(E + V^3 log p)` time and `O(V^2)` auxiliary storage.

This slice does **not** claim:

- arbitrary-precision integer arborescence counts;
- weighted directed Matrix-Tree sums;
- sparse determinant acceleration or fast matrix multiplication;
- minimum-cost arborescence optimization (already covered separately);
- enumeration of all arborescence witnesses;
- a canonical SPQR/cut-tree or any unrelated graph decomposition.

## Scope

Exactly four paths are intended to differ from the live base:

- `include/algorithms/graphs/directed_arborescence_count.hpp`;
- `tests/test_directed_arborescence_count_cases.hpp`;
- one test-registration include in `tests/test_main.cpp`;
- `docs/scope_recovery_directed_arborescence_count.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, length-limited-Huffman, or temporary-file churn is part of this recovery checkpoint.
