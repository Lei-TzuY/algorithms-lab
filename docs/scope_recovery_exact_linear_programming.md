# Scope recovery: exact canonical linear programming

## Coverage decision

After exact minimum-mean directed cycle reached merged-main green, a fresh live
coverage audit deliberately leaves the recent graph/cycle frontier. Repository
code search and pull-request history contain no simplex or linear-programming
implementation, while several superficially attractive alternatives (including
matroid intersection and van Emde Boas trees) are already present. Canonical
linear programming therefore adds a distinct optimization proof model rather
than another adjacent graph variant.

This slice does not resume the frozen Phase 45-69 compiler/backend sequence and
does not normalize the historically stale `ROADMAP.md`. Prospective scope remains
governed by `scope_recovery_after_phase69.md` plus live coverage audits.

## Public problem and exactness boundary

The public solver accepts signed 64-bit integer data for the canonical form

```
maximize     c^T x
subject to   A x <= b
             x >= 0.
```

It returns one of three exact statuses:

- `Optimal`: a reduced-rational primal solution and exact objective value;
- `Infeasible`: Phase I proves that no feasible canonical solution exists;
- `Unbounded`: a current feasible point plus a reduced-rational recession ray
  `d` with `d >= 0`, `A d <= 0`, and `c^T d > 0`.

`Rational64` stores a signed 64-bit numerator and positive signed 64-bit
denominator. Tableau operations use checked, reduced rational arithmetic. Cross
cancellation is applied before multiplication and rational ordering uses an
Euclidean/continued-fraction comparison rather than unchecked cross products.
If an exact intermediate required by this bounded representation cannot be
stored, production throws `std::overflow_error` instead of wrapping, rounding,
or silently changing the optimization problem.

## Two-phase simplex invariant

The implementation uses a first-principles tableau with explicit basic and
nonbasic variable identities. A pivot is a reversible change of basis preserving
all represented affine equalities. Deterministic variable-id tie breaking is used
for entering/leaving choices so repeated solves are replayable and degenerate
ties do not depend on container iteration order.

If an initial right-hand side is negative, Phase I introduces one artificial
variable and pivots it into the most-negative row. The auxiliary objective is
optimized exactly. A zero auxiliary optimum is the feasibility obligation; any
nonzero optimum yields `Infeasible`. A remaining zero-valued artificial basic
variable is pivoted out when a nonzero eligible coefficient exists.

Phase II optimizes the requested objective from the feasible basis. If every
reduced cost is nonnegative, the current primal basic solution is optimal. If an
entering column has improving reduced cost but no row with a positive pivot
coefficient, the objective is unbounded in that direction. Production reconstructs
an original-variable recession ray from that tableau column instead of returning
an unqualified status bit.

## Verification

Focused pre-upload verification passes repository-equivalent strict warning flags
under GCC and Clang plus an actual GCC ASan+UBSan build.

Deterministic cases cover:

- integral and fractional optima;
- Phase-I feasibility recovery from negative right-hand sides;
- infeasibility;
- unconstrained and constrained unboundedness with replayed rays;
- zero-variable/zero-constraint semantics;
- shape validation; and
- fail-closed arithmetic at a signed 64-bit objective boundary.

The primary differential evidence is independent of tableau pivoting:

1. **1,500 one-variable programs** use interval intersection to classify
   feasibility/unboundedness and derive the exact optimum. This corpus exercises
   all three public statuses. Every production optimal point is replayed against
   every constraint; every unbounded ray is checked against `d >= 0`, `A d <= 0`,
   and `c^T d > 0`.
2. **900 bounded two-variable programs** include explicit box constraints and
   random additional half-spaces. An independent oracle enumerates every pair of
   boundary lines (including the nonnegativity axes), solves each intersection
   directly, filters feasible vertices, and chooses the exact best objective.
   It contains no simplex tableau, basis, reduced-cost, or pivot recurrence.

The universal simplex optimality/feasibility theorems remain proof obligations;
finite randomized tests are implementation evidence, not a theorem substitute.

## Complexity and non-claims

For `m` constraints and `n` variables, this direct tableau stores
`O((m+n)^2)` rational state. Each pivot updates `O(mn)` entries. The number of
simplex pivots is not polynomially bounded for this pivot rule in the worst case,
so production deliberately does **not** claim polynomial-time simplex behavior.
Arithmetic cost also depends on exact numerator/denominator sizes; `Rational64`
provides a bounded exact domain rather than arbitrary precision.

This checkpoint intentionally stops at canonical exact linear programming. It
does not claim interior-point methods, mixed-integer programming, arbitrary-
precision rationals, numerical floating-point robustness, dual/Farkas certificate
APIs, or a generic convex-optimization framework.
