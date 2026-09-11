# Scope recovery: exact general CNF SAT with deterministic DPLL

## Coverage decision

A fresh live-code and pull-request-history audit after exact chromatic-number
search found no general CNF SAT, DPLL, or unit-propagation solver. The repository
already has a sealed polynomial-time 2-SAT solver based on implication-graph SCCs,
but that capability is intentionally restricted to clauses of size two and does
not solve arbitrary CNF.

This recovery slice deliberately changes proof model after DSATUR graph coloring
rather than continuing the recent graph-cycle / embedding frontier. It adds a
first-principles exact Boolean search with reversible assignments and an
independent complete truth-table oracle.

## Production contract

`algorithms::constraints::solve_cnf_sat` accepts an explicit variable count and
arbitrary-length CNF clauses over `CnfLiteral {variable, positive}`.

- variable ids must lie in `[0, variable_count)`; invalid ids throw
  `std::out_of_range`
- the empty formula is satisfiable; an empty clause makes the formula
  unsatisfiable
- duplicate literals and duplicate clauses are removed
- tautological clauses containing both polarities of one variable are discarded
- literals and clauses are sorted before search, making clause/literal input order
  semantically irrelevant and deterministic
- SAT returns a complete replayable assignment; variables unconstrained by the
  formula are deterministically filled with `false`
- UNSAT returns `satisfiable == false` and an empty assignment
- diagnostics expose search-node, decision, and unit-assignment counts; counter
  overflow fails closed

## Algorithm and proof boundary

Production repeatedly scans the normalized formula for unit clauses. Whenever a
clause is unsatisfied and has exactly one unassigned literal, that literal is
forced in every satisfying extension of the current partial assignment and is
recorded on a reversible assignment trail. A fully false clause is an immediate
conflict.

After unit propagation reaches a fixed point, production chooses the smallest
unassigned variable and explores `false` before `true`. These two branches
partition every remaining total assignment, so recursively exhausting both is a
complete satisfiability decision procedure. Trail checkpoints roll back unit and
decision assignments without copying the full assignment vector at every search
node.

Normalization preserves satisfiability: duplicate literals/clauses do not change
a disjunction/conjunction, and a tautological clause is always true. Therefore a
SAT witness for the normalized formula also satisfies the original formula.

This is exact DPLL search, not a claim that tests prove the general SAT theorem.
UNSAT is established by exhaustive branching of this search; the public API does
not return a resolution/DRAT proof certificate.

## Verification

Focused repo-native verification of the exact candidate passed:

- GCC C++20 strict warnings-as-errors: 6/6
- Clang C++20 strict warnings-as-errors: 6/6
- actual GCC ASan+UBSan with leak detection: 6/6

Deterministic cases cover empty formulas, explicit empty-clause UNSAT, variable
validation, duplicate literals and clauses, tautology elimination, clause/literal
reordering, chained unit propagation, unused-variable completion, a case that
requires backtracking, a deterministic UNSAT case, and repeated execution.

The primary randomized oracle is independent of normalization, unit propagation,
and DPLL branching: 700 fixed-seed formulas with `0..8` variables and arbitrary
clauses of length `0..6` are decided by enumerating every total truth assignment.
Production SAT/UNSAT status must match exactly, and every production SAT witness
is replayed against the original unnormalized clauses.

Focused integration testing also caught a real candidate defect before upload:
diagnostics counters had lost explicit zero initialization during an API refactor,
which made deterministic-result replay unstable. The production counters were
fixed; the deterministic equality regression was retained rather than weakened.

## Complexity and non-claims

For `V` variables and `L` literal occurrences in the normalized formula, the
direct repeated-scan implementation uses a conservative worst-case bound of
`O(2^V * V * L)` time. Resident normalized formula, assignment, and trail state
are `O(V + L)`, excluding the `O(V)` recursion stack.

No CDCL, watched-literal, clause-learning, restart, incremental-SAT, Max-SAT,
weighted-SAT, proof-certificate, or industrial-performance claim is made. The
sealed SCC-based 2-SAT solver remains the polynomial specialized algorithm for
2-CNF; this slice intentionally exposes the different general-CNF exponential
boundary.

## Scope

The intended review surface is four files: one new header-only production API,
one new repo-native test translation unit, one focused recovery proof document,
and one CMake test-registration line. Existing 2-SAT production/tests, README,
ROADMAP, recovery-authority documentation, workflows, benchmarks, compiler/backend
surfaces, and unrelated algorithms are untouched.
