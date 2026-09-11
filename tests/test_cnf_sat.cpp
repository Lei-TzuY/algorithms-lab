#include "algorithms/constraints/cnf_sat.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::constraints::CnfClause;
using algorithms::constraints::CnfLiteral;
using algorithms::constraints::CnfSatResult;
using algorithms::constraints::solve_cnf_sat;

CnfLiteral cnf_literal(std::size_t variable, bool positive) {
  return CnfLiteral{variable, positive};
}

bool cnf_satisfies(const std::vector<CnfClause>& clauses,
               const std::vector<bool>& assignment) {
  for (const CnfClause& clause : clauses) {
    bool clause_satisfied = false;
    for (const CnfLiteral value : clause) {
      if (assignment[value.variable] == value.positive) {
        clause_satisfied = true;
        break;
      }
    }
    if (!clause_satisfied) {
      return false;
    }
  }
  return true;
}

bool exhaustive_satisfiable(std::size_t variable_count,
                            const std::vector<CnfClause>& clauses) {
  const std::size_t assignment_count = std::size_t{1} << variable_count;
  for (std::size_t mask = 0; mask < assignment_count; ++mask) {
    std::vector<bool> assignment(variable_count, false);
    for (std::size_t variable = 0; variable < variable_count; ++variable) {
      assignment[variable] = ((mask >> variable) & 1U) != 0U;
    }
    if (cnf_satisfies(clauses, assignment)) {
      return true;
    }
  }
  return false;
}

TEST_CASE(cnf_sat_empty_formula_empty_clause_and_validation) {
  const std::vector<CnfClause> empty_formula;
  const CnfSatResult empty = solve_cnf_sat(0, empty_formula);
  REQUIRE(empty.satisfiable);
  REQUIRE(empty.assignment.empty());

  const std::vector<CnfClause> empty_clause{CnfClause{}};
  const CnfSatResult impossible = solve_cnf_sat(0, empty_clause);
  REQUIRE(!impossible.satisfiable);
  REQUIRE(impossible.assignment.empty());

  const std::vector<CnfClause> invalid{{cnf_literal(1, true)}};
  REQUIRE_THROWS_AS(solve_cnf_sat(1, invalid), std::out_of_range);
}

TEST_CASE(cnf_sat_normalizes_duplicates_tautologies_and_input_order) {
  std::vector<CnfClause> clauses{
      {cnf_literal(2, true), cnf_literal(0, false), cnf_literal(2, true)},
      {cnf_literal(1, true), cnf_literal(1, false)},
      {cnf_literal(0, true), cnf_literal(2, false)},
      {cnf_literal(2, true), cnf_literal(0, false)}};
  const CnfSatResult first = solve_cnf_sat(3, clauses);
  REQUIRE(first.satisfiable);
  REQUIRE(cnf_satisfies(clauses, first.assignment));

  std::reverse(clauses.begin(), clauses.end());
  for (CnfClause& clause : clauses) {
    std::reverse(clause.begin(), clause.end());
  }
  const CnfSatResult second = solve_cnf_sat(3, clauses);
  REQUIRE_EQ(first, second);
}

TEST_CASE(cnf_sat_unit_propagation_chain_and_unused_variables) {
  const std::vector<CnfClause> clauses{
      {cnf_literal(0, true)},
      {cnf_literal(0, false), cnf_literal(1, true)},
      {cnf_literal(1, false), cnf_literal(2, true)}};
  const CnfSatResult result = solve_cnf_sat(5, clauses);
  REQUIRE(result.satisfiable);
  REQUIRE_EQ(result.assignment.size(), 5U);
  REQUIRE(result.assignment[0]);
  REQUIRE(result.assignment[1]);
  REQUIRE(result.assignment[2]);
  REQUIRE(!result.assignment[3]);
  REQUIRE(!result.assignment[4]);
  REQUIRE(result.diagnostics.unit_assignments >= 3U);
  REQUIRE(cnf_satisfies(clauses, result.assignment));
}

TEST_CASE(cnf_sat_backtracks_and_detects_unsatisfiable_formulas) {
  const std::vector<CnfClause> backtracking{
      {cnf_literal(0, true), cnf_literal(1, true)},
      {cnf_literal(0, true), cnf_literal(1, false)},
      {cnf_literal(0, false), cnf_literal(1, false)}};
  const CnfSatResult sat = solve_cnf_sat(2, backtracking);
  REQUIRE(sat.satisfiable);
  REQUIRE(cnf_satisfies(backtracking, sat.assignment));
  REQUIRE(sat.diagnostics.decisions >= 1U);
  REQUIRE(sat.diagnostics.search_nodes >= 3U);

  const std::vector<CnfClause> unsat{
      {cnf_literal(0, true)},
      {cnf_literal(1, true)},
      {cnf_literal(0, false), cnf_literal(1, false)}};
  const CnfSatResult no_solution = solve_cnf_sat(2, unsat);
  REQUIRE(!no_solution.satisfiable);
  REQUIRE(no_solution.assignment.empty());
}

TEST_CASE(cnf_sat_repeat_execution_is_deterministic) {
  const std::vector<CnfClause> clauses{
      {cnf_literal(0, true), cnf_literal(1, true), cnf_literal(2, false)},
      {cnf_literal(0, false), cnf_literal(2, true)},
      {cnf_literal(1, false), cnf_literal(3, true)},
      {cnf_literal(2, false), cnf_literal(3, false)}};
  const CnfSatResult first = solve_cnf_sat(4, clauses);
  const CnfSatResult second = solve_cnf_sat(4, clauses);
  REQUIRE_EQ(first, second);
  REQUIRE(first.satisfiable);
  REQUIRE(cnf_satisfies(clauses, first.assignment));
}

TEST_CASE(cnf_sat_randomized_differential_against_truth_table) {
  std::mt19937_64 generator(0xD0115A7ULL);
  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const std::size_t variable_count = generator() % 9U;
    const std::size_t clause_count = generator() % 16U;
    std::vector<CnfClause> clauses;
    clauses.reserve(clause_count);
    for (std::size_t clause_index = 0; clause_index < clause_count;
         ++clause_index) {
      const std::size_t literal_count =
          variable_count == 0U ? 0U : generator() % 7U;
      CnfClause clause;
      clause.reserve(literal_count);
      for (std::size_t index = 0; index < literal_count; ++index) {
        clause.push_back(cnf_literal(generator() % variable_count,
                                 (generator() & 1U) != 0U));
      }
      clauses.push_back(std::move(clause));
    }

    const bool expected = exhaustive_satisfiable(variable_count, clauses);
    const CnfSatResult result = solve_cnf_sat(variable_count, clauses);
    REQUIRE_EQ(result.satisfiable, expected);
    if (result.satisfiable) {
      REQUIRE_EQ(result.assignment.size(), variable_count);
      REQUIRE(cnf_satisfies(clauses, result.assignment));
    } else {
      REQUIRE(result.assignment.empty());
    }
  }
}

}  // namespace
