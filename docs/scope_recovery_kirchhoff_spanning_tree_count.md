# Scope recovery: Kirchhoff spanning-tree count over a prime field

## Coverage decision

A fresh live-code, branch, and recent-commit audit after complete prime-field
irreducible factorization found no Matrix-Tree / Kirchhoff spanning-tree counting
capability. The latest recovery work was concentrated in polynomial factorization,
so this slice deliberately changes proof model and subsystem rather than farming
another factorization variant.

The new public contract is
`spanning_tree_count_mod_prime(graph, prime)`: the exact spanning-tree count of an
undirected multigraph reduced modulo a caller-supplied prime.

## Graph semantics

- directed graphs are rejected;
- self-loops are ignored because they cannot belong to a spanning tree;
- parallel logical edges are distinct choices and therefore contribute their
  multiplicity to the count;
- stored edge weights are intentionally ignored: this is cardinality counting,
  not a weighted tree-enumerator;
- the empty graph uses the explicit repository convention `0`, while a singleton
  graph has one spanning tree;
- the modulus must be prime and is validated through the sealed full-width
  deterministic primality routine.

The result is an exact residue in `F_p`. It is **not** an arbitrary-precision
integer count reconstructed from several primes.

## Kirchhoff proof obligation

For an undirected multigraph, build the Laplacian `L` where every non-loop edge
copy increments the two endpoint diagonal entries and decrements the two
corresponding off-diagonal entries. Parallel edges repeat those updates; loops
contribute nothing.

Kirchhoff's Matrix-Tree theorem states that deleting any one row and matching
column from `L` yields a cofactor whose determinant equals the number of spanning
trees. Reducing that cofactor modulo prime `p` therefore preserves the desired
count modulo `p`.

Production deletes the final row/column and evaluates the determinant with
first-principles Gaussian elimination over `F_p`. Pivot swaps flip the sign;
pivot multiplication accumulates the determinant; lower rows are eliminated
with overflow-safe modular multiplication and Fermat inverses. A missing pivot
returns zero, exactly matching disconnected/singular cases.

The theorem supplies the universal correctness argument. Finite tests are
implementation evidence, not a proof of Matrix-Tree itself.

## Verification

Deterministic evidence covers:

- directed/composite-modulus rejection;
- empty and singleton semantics;
- self-loop neutrality;
- parallel-edge multiplicity with weights deliberately ignored;
- triangle, 4-cycle, disconnected graph, and complete-graph known counts;
- a full-width prime modulus (`18446744073709551557`).

The primary randomized oracle is independent of determinants. For 500 fixed-seed
multigraphs with at most seven vertices and twelve inserted edge copies, it
enumerates every non-loop edge subset of size `V-1`, rebuilds the selected graph,
and counts connected subsets directly. Production must equal this exhaustive
count modulo a randomly selected small prime.

Focused candidate verification passes strict GCC, strict Clang, and actual
ASan+UBSan builds.

## Complexity and non-claims

With `V` vertices and `E` logical non-loop edges, Laplacian construction is
`O(E)`. The direct dense elimination performs `O(V^3)` field-operation slots;
because repository modular multiplication is overflow-safe repeated doubling,
a conservative bit-cost statement is `O(E + V^3 log p)` time with `O(V^2)`
auxiliary storage.

No arbitrary-precision exact integer count, weighted Matrix-Tree theorem,
directed arborescence count, sparse determinant optimization, fast matrix
multiplication, or asymptotically optimal determinant claim is made.
