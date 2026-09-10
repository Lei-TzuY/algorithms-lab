# Scope recovery: 2-SAT implication-graph solving

## Coverage decision

After the merged undirected low-link checkpoint, a fresh coverage audit deliberately
moves away from the recent geometry, cut, shortest-path, and DFS-structure streaks.
The repository already contains exact SCC decomposition but no Boolean
constraint-satisfaction layer. 2-SAT therefore adds a distinct algorithmic proof
boundary while reusing a sealed graph primitive instead of duplicating SCC logic.

This slice does not resume the frozen Phase 45-69 compiler/backend sequence and
does not normalize the historically stale `ROADMAP.md`. Prospective scope remains
governed by `scope_recovery_after_phase69.md` plus live coverage audits.

## Representation and reduction

Variable `v` owns two implication vertices:

- `2*v` is the negative literal `!v`;
- `2*v+1` is the positive literal `v`;
- negation is therefore `node ^ 1`.

Each clause `(a OR b)` contributes the logically equivalent implications
`!a -> b` and `!b -> a`. Production sorts and deduplicates the complete edge set
before constructing `Graph`, so duplicate clauses and clause ordering cannot
change graph insertion order, SCC traversal, assignment choice, or contradiction
witnesses.

Literal and edge-count arithmetic is checked before the doubled graph/edge counts
are formed. Clause variables outside `[0, variable_count)` are rejected.

## SAT invariant and assignment

Let `S(x)` be the SCC containing literal `x`. A 2-CNF formula is satisfiable iff,
for every variable `v`, `S(v) != S(!v)`. Production reuses the sealed Tarjan SCC
implementation and contracts the implication graph through the existing
`condensation_graph` helper.

The condensation is a DAG. Production takes the existing deterministic
topological ordering and assigns `v = true` exactly when `S(v)` appears later
than `S(!v)`. For every cross-component implication `a -> b`, the topological
rank of `S(a)` is smaller than the rank of `S(b)`; the later-component rule is
the standard SCC 2-SAT assignment argument. Tests replay every returned SAT
assignment directly against every input clause rather than relying on SCC labels.

## UNSAT certificate

If both literals of a variable occupy one SCC, each reaches the other. Production
selects the smallest contradictory variable and returns two explicit literal
paths:

- `!v => v`;
- `v => !v`.

Each path is found by BFS restricted to the contradictory SCC. Replaying every
consecutive path edge against the independently reconstructed implication edge
set certifies the mutual implication that forces contradiction. The public UNSAT
result returns no assignment.

## Verification

Focused pre-upload verification passed under repository-equivalent strict flags
with GCC, Clang, and an actual GCC ASan+UBSan build.

Deterministic cases cover the empty formula, positive/negative unit clauses,
tautologies, duplicate clauses, invalid variables, direct contradiction, multiple
contradictory variables with smallest-variable witness selection, and clause-order
canonicalization.

The differential corpus uses fixed seed `0x2A7A11`: 1,500 formulas with 1-8
variables and 0-24 clauses. An independent truth-table oracle enumerates all
`2^V` assignments. SAT results must agree with the oracle and satisfy every
clause; UNSAT results must agree with the oracle and replay both certificate
paths. Every formula is also shuffled and solved again to verify the canonical
result is independent of clause order.

The exhaustive oracle contains no SCC, implication-graph reachability, or
condensation logic and therefore does not mirror the production recurrence.

## Complexity and boundary

For `V` Boolean variables and `C` clauses, implication canonicalization costs
`O(C log C)`. SCC, witness BFS, and DAG processing are linear in the resulting
`O(V+C)` graph, while the existing condensation helper performs deterministic
edge deduplication with logarithmic set operations. The direct public bound is
therefore `O(V + C log C)` time and `O(V + C)` storage, excluding recursion stack
inside the sealed Tarjan SCC implementation.

This checkpoint intentionally stops at exact 2-SAT. It does not claim a general
SAT solver, CDCL, Horn-SAT, Max-SAT, SMT, or a generic constraint framework.
