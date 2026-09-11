#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::constraints {

struct CnfLiteral {
  std::size_t variable;
  bool positive;

  friend bool operator==(const CnfLiteral&, const CnfLiteral&) = default;
};

using CnfClause = std::vector<CnfLiteral>;

struct CnfSatDiagnostics {
  std::size_t search_nodes{};
  std::size_t decisions{};
  std::size_t unit_assignments{};

  friend bool operator==(const CnfSatDiagnostics&,
                         const CnfSatDiagnostics&) = default;
};

struct CnfSatResult {
  bool satisfiable;
  std::vector<bool> assignment;
  CnfSatDiagnostics diagnostics;

  friend bool operator==(const CnfSatResult&, const CnfSatResult&) = default;
};

namespace detail {

using CnfAssignment = std::vector<std::int8_t>;
using CnfFormula = std::vector<CnfClause>;
using CnfTrail = std::vector<std::size_t>;

inline void cnf_checked_increment(std::size_t& counter,
                                  const char* message) {
  if (counter == std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error(message);
  }
  ++counter;
}

[[nodiscard]] inline bool cnf_literal_less(const CnfLiteral& left,
                                           const CnfLiteral& right) noexcept {
  if (left.variable != right.variable) {
    return left.variable < right.variable;
  }
  return static_cast<unsigned>(left.positive) <
         static_cast<unsigned>(right.positive);
}

[[nodiscard]] inline bool cnf_clause_less(const CnfClause& left,
                                          const CnfClause& right) {
  return std::lexicographical_compare(left.begin(), left.end(), right.begin(),
                                      right.end(), cnf_literal_less);
}

[[nodiscard]] inline CnfFormula cnf_normalize_formula(
    std::size_t variable_count, std::span<const CnfClause> input) {
  CnfFormula formula;
  formula.reserve(input.size());

  for (const CnfClause& raw_clause : input) {
    CnfClause clause = raw_clause;
    for (const CnfLiteral literal : clause) {
      if (literal.variable >= variable_count) {
        throw std::out_of_range("CNF literal variable out of range");
      }
    }

    std::sort(clause.begin(), clause.end(), cnf_literal_less);
    clause.erase(std::unique(clause.begin(), clause.end()), clause.end());

    bool tautology = false;
    for (std::size_t index = 1; index < clause.size(); ++index) {
      if (clause[index - 1].variable == clause[index].variable &&
          clause[index - 1].positive != clause[index].positive) {
        tautology = true;
        break;
      }
    }
    if (!tautology) {
      formula.push_back(std::move(clause));
    }
  }

  std::sort(formula.begin(), formula.end(), cnf_clause_less);
  formula.erase(std::unique(formula.begin(), formula.end()), formula.end());
  return formula;
}

[[nodiscard]] inline bool cnf_literal_is_true(CnfLiteral literal,
                                              std::int8_t value) noexcept {
  return (value == 1) == literal.positive;
}

inline void cnf_rollback(CnfAssignment& assignment, CnfTrail& trail,
                         std::size_t checkpoint) {
  while (trail.size() > checkpoint) {
    assignment[trail.back()] = static_cast<std::int8_t>(-1);
    trail.pop_back();
  }
}

[[nodiscard]] inline bool cnf_assign(CnfAssignment& assignment, CnfTrail& trail,
                                     std::size_t variable,
                                     std::int8_t value) {
  std::int8_t& slot = assignment[variable];
  if (slot >= 0) {
    return slot == value;
  }
  slot = value;
  trail.push_back(variable);
  return true;
}

struct CnfPropagationResult {
  bool conflict;
  bool all_satisfied;
};

[[nodiscard]] inline CnfPropagationResult cnf_propagate_units(
    const CnfFormula& formula, CnfAssignment& assignment, CnfTrail& trail,
    CnfSatDiagnostics& diagnostics) {
  for (;;) {
    bool changed = false;
    bool every_clause_satisfied = true;

    for (const CnfClause& clause : formula) {
      bool clause_satisfied = false;
      std::size_t unassigned_count = 0;
      CnfLiteral last_unassigned{};

      for (const CnfLiteral literal : clause) {
        const std::int8_t value = assignment[literal.variable];
        if (value < 0) {
          ++unassigned_count;
          last_unassigned = literal;
          continue;
        }
        if (cnf_literal_is_true(literal, value)) {
          clause_satisfied = true;
          break;
        }
      }

      if (clause_satisfied) {
        continue;
      }

      every_clause_satisfied = false;
      if (unassigned_count == 0) {
        return CnfPropagationResult{true, false};
      }
      if (unassigned_count == 1) {
        const std::int8_t required = last_unassigned.positive ? 1 : 0;
        const bool was_unassigned = assignment[last_unassigned.variable] < 0;
        if (!cnf_assign(assignment, trail, last_unassigned.variable, required)) {
          return CnfPropagationResult{true, false};
        }
        if (was_unassigned) {
          cnf_checked_increment(diagnostics.unit_assignments,
                                "CNF SAT unit-assignment counter overflow");
          changed = true;
        }
      }
    }

    if (every_clause_satisfied) {
      return CnfPropagationResult{false, true};
    }
    if (!changed) {
      return CnfPropagationResult{false, false};
    }
  }
}

[[nodiscard]] inline bool cnf_search(const CnfFormula& formula,
                                     CnfAssignment& assignment,
                                     CnfTrail& trail,
                                     CnfSatDiagnostics& diagnostics) {
  cnf_checked_increment(diagnostics.search_nodes,
                        "CNF SAT search-node counter overflow");
  const std::size_t node_checkpoint = trail.size();
  const CnfPropagationResult propagation =
      cnf_propagate_units(formula, assignment, trail, diagnostics);
  if (propagation.conflict) {
    cnf_rollback(assignment, trail, node_checkpoint);
    return false;
  }
  if (propagation.all_satisfied) {
    return true;
  }

  std::size_t variable = assignment.size();
  for (std::size_t index = 0; index < assignment.size(); ++index) {
    if (assignment[index] < 0) {
      variable = index;
      break;
    }
  }
  if (variable == assignment.size()) {
    cnf_rollback(assignment, trail, node_checkpoint);
    return false;
  }

  cnf_checked_increment(diagnostics.decisions,
                        "CNF SAT decision counter overflow");
  const std::size_t branch_checkpoint = trail.size();

  static_cast<void>(cnf_assign(assignment, trail, variable, 0));
  if (cnf_search(formula, assignment, trail, diagnostics)) {
    return true;
  }
  cnf_rollback(assignment, trail, branch_checkpoint);

  static_cast<void>(cnf_assign(assignment, trail, variable, 1));
  if (cnf_search(formula, assignment, trail, diagnostics)) {
    return true;
  }
  cnf_rollback(assignment, trail, node_checkpoint);
  return false;
}

}  // namespace detail

// Exact deterministic DPLL for general CNF.
//
// Input is canonicalized before search: duplicate literals/clauses are removed,
// tautological clauses are discarded, and remaining clauses/literals are sorted.
// Search repeatedly performs unit propagation, then branches on the smallest
// unassigned variable, trying false before true. SAT results include a concrete
// replayable assignment; UNSAT is established by exhausting both branches.
//
// Worst case remains exponential. For V variables and L normalized literal
// occurrences, this direct repeated-scan implementation is conservatively
// O(2^V * V * L) time. Resident normalized formula/assignment/trail state is
// O(V + L), excluding O(V) recursion stack frames.
[[nodiscard]] inline CnfSatResult solve_cnf_sat(
    std::size_t variable_count, std::span<const CnfClause> clauses) {
  const detail::CnfFormula formula =
      detail::cnf_normalize_formula(variable_count, clauses);
  detail::CnfAssignment assignment(variable_count,
                                   static_cast<std::int8_t>(-1));
  detail::CnfTrail trail;
  trail.reserve(variable_count);
  CnfSatDiagnostics diagnostics{};

  if (!detail::cnf_search(formula, assignment, trail, diagnostics)) {
    return CnfSatResult{false, {}, diagnostics};
  }

  std::vector<bool> witness(variable_count, false);
  for (std::size_t index = 0; index < assignment.size(); ++index) {
    if (assignment[index] > 0) {
      witness[index] = true;
    }
  }
  return CnfSatResult{true, std::move(witness), diagnostics};
}

}  // namespace algorithms::constraints
