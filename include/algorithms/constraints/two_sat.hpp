#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::constraints {

struct TwoSatLiteral {
  std::size_t variable;
  bool positive;

  friend bool operator==(const TwoSatLiteral&, const TwoSatLiteral&) = default;
};

struct TwoSatClause {
  TwoSatLiteral first;
  TwoSatLiteral second;

  friend bool operator==(const TwoSatClause&, const TwoSatClause&) = default;
};

struct TwoSatContradictionWitness {
  std::size_t variable;
  std::vector<TwoSatLiteral> false_implies_true;
  std::vector<TwoSatLiteral> true_implies_false;

  friend bool operator==(const TwoSatContradictionWitness&,
                         const TwoSatContradictionWitness&) = default;
};

struct TwoSatResult {
  bool satisfiable;
  std::vector<bool> assignment;
  std::optional<TwoSatContradictionWitness> contradiction;
};

// Solves a 2-CNF formula. Clauses and duplicate clauses may be supplied in any
// order; production canonicalizes the implication edge set before SCC analysis.
//
// SAT invariant: no variable's two literal vertices share an SCC. Assigning the
// literal whose SCC appears later in a topological order of the condensation DAG
// makes every implication, hence every clause, true.
//
// UNSAT certificate: for the smallest contradictory variable x, both returned
// paths are real implication paths inside one SCC: !x => x and x => !x.
//
// Time: O(V + C log C) due implication canonicalization; space: O(V + C),
// excluding recursion used by the sealed Tarjan SCC implementation.
[[nodiscard]] TwoSatResult solve_two_sat(
    std::size_t variable_count, std::span<const TwoSatClause> clauses);

}  // namespace algorithms::constraints
