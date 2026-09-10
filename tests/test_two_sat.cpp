#include "algorithms/constraints/two_sat.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::constraints::TwoSatClause;
using algorithms::constraints::TwoSatContradictionWitness;
using algorithms::constraints::TwoSatLiteral;
using algorithms::constraints::TwoSatResult;
using algorithms::constraints::solve_two_sat;

TwoSatLiteral literal(std::size_t variable, bool positive) {
  return TwoSatLiteral{variable, positive};
}

bool evaluate_literal(const TwoSatLiteral value,
                      const std::vector<bool>& assignment) {
  return assignment[value.variable] == value.positive;
}

bool satisfies(const std::vector<TwoSatClause>& clauses,
               const std::vector<bool>& assignment) {
  for (const TwoSatClause& clause : clauses) {
    if (!evaluate_literal(clause.first, assignment) &&
        !evaluate_literal(clause.second, assignment)) {
      return false;
    }
  }
  return true;
}

bool exhaustive_satisfiable(std::size_t variable_count,
                            const std::vector<TwoSatClause>& clauses) {
  const std::size_t assignment_count = std::size_t{1} << variable_count;
  for (std::size_t mask = 0; mask < assignment_count; ++mask) {
    std::vector<bool> assignment(variable_count, false);
    for (std::size_t variable = 0; variable < variable_count; ++variable) {
      assignment[variable] = ((mask >> variable) & 1U) != 0U;
    }
    if (satisfies(clauses, assignment)) {
      return true;
    }
  }
  return false;
}

std::size_t literal_node(const TwoSatLiteral value) {
  return value.variable * 2U + (value.positive ? 1U : 0U);
}

std::vector<std::pair<std::size_t, std::size_t>> implication_edges(
    const std::vector<TwoSatClause>& clauses) {
  std::vector<std::pair<std::size_t, std::size_t>> result;
  for (const TwoSatClause& clause : clauses) {
    const std::size_t first = literal_node(clause.first);
    const std::size_t second = literal_node(clause.second);
    result.emplace_back(first ^ 1U, second);
    result.emplace_back(second ^ 1U, first);
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

void require_implication_path(
    const std::vector<TwoSatLiteral>& path,
    const std::vector<std::pair<std::size_t, std::size_t>>& edges) {
  REQUIRE(path.size() >= 2U);
  for (std::size_t index = 1; index < path.size(); ++index) {
    const std::pair<std::size_t, std::size_t> edge{
        literal_node(path[index - 1U]), literal_node(path[index])};
    REQUIRE(std::binary_search(edges.begin(), edges.end(), edge));
  }
}

void require_contradiction_witness(
    const TwoSatContradictionWitness& witness,
    const std::vector<TwoSatClause>& clauses) {
  const auto edges = implication_edges(clauses);
  REQUIRE_EQ(witness.false_implies_true.front(),
             literal(witness.variable, false));
  REQUIRE_EQ(witness.false_implies_true.back(),
             literal(witness.variable, true));
  REQUIRE_EQ(witness.true_implies_false.front(),
             literal(witness.variable, true));
  REQUIRE_EQ(witness.true_implies_false.back(),
             literal(witness.variable, false));
  require_implication_path(witness.false_implies_true, edges);
  require_implication_path(witness.true_implies_false, edges);
}

TEST_CASE(two_sat_empty_units_tautology_and_validation) {
  {
    const std::vector<TwoSatClause> clauses;
    const TwoSatResult result = solve_two_sat(0, clauses);
    REQUIRE(result.satisfiable);
    REQUIRE(result.assignment.empty());
    REQUIRE(!result.contradiction.has_value());
  }
  {
    const std::vector<TwoSatClause> clauses{
        {literal(0, true), literal(0, true)}};
    const TwoSatResult result = solve_two_sat(1, clauses);
    REQUIRE(result.satisfiable);
    REQUIRE(result.assignment[0]);
  }
  {
    const std::vector<TwoSatClause> clauses{
        {literal(0, false), literal(0, false)}};
    const TwoSatResult result = solve_two_sat(1, clauses);
    REQUIRE(result.satisfiable);
    REQUIRE(!result.assignment[0]);
  }
  {
    const std::vector<TwoSatClause> clauses{
        {literal(0, true), literal(0, false)}};
    const TwoSatResult result = solve_two_sat(1, clauses);
    REQUIRE(result.satisfiable);
    REQUIRE(satisfies(clauses, result.assignment));
  }
  const std::vector<TwoSatClause> invalid{
      {literal(1, true), literal(0, false)}};
  REQUIRE_THROWS_AS(solve_two_sat(1, invalid), std::out_of_range);
}

TEST_CASE(two_sat_unsat_returns_replayable_mutual_implication_witness) {
  const std::vector<TwoSatClause> clauses{
      {literal(0, true), literal(0, true)},
      {literal(0, false), literal(0, false)}};
  const TwoSatResult result = solve_two_sat(1, clauses);
  REQUIRE(!result.satisfiable);
  REQUIRE(result.assignment.empty());
  REQUIRE(result.contradiction.has_value());
  REQUIRE_EQ(result.contradiction->variable, 0U);
  require_contradiction_witness(*result.contradiction, clauses);
}

TEST_CASE(two_sat_selects_smallest_contradictory_variable) {
  const std::vector<TwoSatClause> clauses{
      {literal(1, true), literal(1, true)},
      {literal(1, false), literal(1, false)},
      {literal(0, false), literal(0, false)},
      {literal(0, true), literal(0, true)}};
  const TwoSatResult result = solve_two_sat(2, clauses);
  REQUIRE(!result.satisfiable);
  REQUIRE(result.contradiction.has_value());
  REQUIRE_EQ(result.contradiction->variable, 0U);
  require_contradiction_witness(*result.contradiction, clauses);
}

TEST_CASE(two_sat_duplicate_and_clause_order_are_canonicalized) {
  std::vector<TwoSatClause> clauses{
      {literal(0, true), literal(1, false)},
      {literal(1, true), literal(2, true)},
      {literal(0, true), literal(1, false)},
      {literal(2, false), literal(0, false)}};
  const TwoSatResult first = solve_two_sat(3, clauses);
  std::reverse(clauses.begin(), clauses.end());
  const TwoSatResult second = solve_two_sat(3, clauses);
  REQUIRE_EQ(first.satisfiable, second.satisfiable);
  REQUIRE_EQ(first.assignment, second.assignment);
  REQUIRE_EQ(first.contradiction, second.contradiction);
  REQUIRE(first.satisfiable);
  REQUIRE(satisfies(clauses, first.assignment));
}

TEST_CASE(two_sat_randomized_differential_against_truth_table) {
  std::mt19937_64 generator(0x2A7A11ULL);
  for (std::size_t trial = 0; trial < 1500U; ++trial) {
    const std::size_t variable_count = 1U + (generator() % 8U);
    const std::size_t clause_count = generator() % 25U;
    std::vector<TwoSatClause> clauses;
    clauses.reserve(clause_count);
    for (std::size_t index = 0; index < clause_count; ++index) {
      clauses.push_back(TwoSatClause{
          literal(generator() % variable_count, (generator() & 1U) != 0U),
          literal(generator() % variable_count, (generator() & 1U) != 0U)});
    }

    const bool expected = exhaustive_satisfiable(variable_count, clauses);
    const TwoSatResult result = solve_two_sat(variable_count, clauses);
    REQUIRE_EQ(result.satisfiable, expected);
    if (result.satisfiable) {
      REQUIRE_EQ(result.assignment.size(), variable_count);
      REQUIRE(satisfies(clauses, result.assignment));
      REQUIRE(!result.contradiction.has_value());
    } else {
      REQUIRE(result.assignment.empty());
      REQUIRE(result.contradiction.has_value());
      require_contradiction_witness(*result.contradiction, clauses);
    }

    std::vector<TwoSatClause> shuffled = clauses;
    std::shuffle(shuffled.begin(), shuffled.end(), generator);
    const TwoSatResult replay = solve_two_sat(variable_count, shuffled);
    REQUIRE_EQ(replay.satisfiable, result.satisfiable);
    REQUIRE_EQ(replay.assignment, result.assignment);
    REQUIRE_EQ(replay.contradiction, result.contradiction);
  }
}

}  // namespace
