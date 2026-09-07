#include "algorithms/optimization/hungarian_assignment.hpp"
#include "algorithms/graphs/min_cost_flow.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::graphs::CostCapacityEdge;
using algorithms::graphs::min_cost_max_flow;
using algorithms::optimization::AssignmentCost;
using algorithms::optimization::AssignmentResult;
using algorithms::optimization::hungarian_min_assignment;

namespace {

AssignmentCost replay_cost(std::size_t rows, std::size_t columns,
                           const std::vector<AssignmentCost>& costs,
                           const AssignmentResult& result) {
  REQUIRE_EQ(result.column_for_row.size(), rows);
  std::vector<bool> used(columns, false);
  AssignmentCost total = 0;
  for (std::size_t row = 0U; row < rows; ++row) {
    const std::size_t column = result.column_for_row[row];
    REQUIRE(column < columns);
    REQUIRE(!used[column]);
    used[column] = true;
    total += costs[row * columns + column];
  }
  return total;
}

AssignmentCost exhaustive_assignment(std::size_t rows, std::size_t columns,
                                     const std::vector<AssignmentCost>& costs) {
  if (rows == 0U) {
    return 0;
  }
  std::vector<std::size_t> columns_order(columns);
  std::iota(columns_order.begin(), columns_order.end(), std::size_t{0});
  AssignmentCost best = std::numeric_limits<AssignmentCost>::max();
  const auto search = [&](auto&& self, std::size_t row,
                          std::vector<bool>& used,
                          AssignmentCost current) -> void {
    if (row == rows) {
      best = std::min(best, current);
      return;
    }
    for (std::size_t column : columns_order) {
      if (!used[column]) {
        used[column] = true;
        self(self, row + 1U, used,
             current + costs[row * columns + column]);
        used[column] = false;
      }
    }
  };
  std::vector<bool> used(columns, false);
  search(search, 0U, used, 0);
  return best;
}

AssignmentCost min_cost_flow_assignment(std::size_t rows, std::size_t columns,
                                        const std::vector<AssignmentCost>& costs) {
  const std::size_t source = 0U;
  const std::size_t first_row = 1U;
  const std::size_t first_column = first_row + rows;
  const std::size_t sink = first_column + columns;
  std::vector<CostCapacityEdge> edges;
  for (std::size_t row = 0U; row < rows; ++row) {
    edges.push_back({source, first_row + row, 1, 0});
    for (std::size_t column = 0U; column < columns; ++column) {
      edges.push_back({first_row + row, first_column + column, 1,
                       costs[row * columns + column]});
    }
  }
  for (std::size_t column = 0U; column < columns; ++column) {
    edges.push_back({first_column + column, sink, 1, 0});
  }
  const auto flow = min_cost_max_flow(sink + 1U, edges, source, sink);
  REQUIRE_EQ(flow.value, static_cast<std::int64_t>(rows));
  return flow.cost;
}

}  // namespace

TEST_CASE(hungarian_assignment_deterministic_rectangular_and_validation) {
  const std::vector<AssignmentCost> costs{
      4, 1, 3, 8,
      2, 0, 5, 7,
      3, 2, 2, 6,
  };
  const auto result = hungarian_min_assignment(3U, 4U, costs);
  REQUIRE_EQ(result.cost, 5);
  REQUIRE_EQ(replay_cost(3U, 4U, costs, result), 5);

  REQUIRE_EQ(hungarian_min_assignment(0U, 0U, {}).cost, 0);
  const std::vector<AssignmentCost> one_value{7};
  REQUIRE_THROWS_AS(hungarian_min_assignment(0U, 1U, one_value),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(hungarian_min_assignment(2U, 1U, std::vector<AssignmentCost>{1, 2}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(hungarian_min_assignment(2U, 2U, std::vector<AssignmentCost>{1, 2, 3}),
                    std::invalid_argument);
}

TEST_CASE(hungarian_assignment_negative_costs_ties_and_checked_arithmetic) {
  const std::vector<AssignmentCost> negative{
      -5, -5, 0,
      -5, -5, 0,
  };
  const auto tied = hungarian_min_assignment(2U, 3U, negative);
  REQUIRE_EQ(tied.cost, -10);
  REQUIRE_EQ(tied.column_for_row, (std::vector<std::size_t>{0U, 1U}));

  const std::vector<AssignmentCost> overflow{
      std::numeric_limits<AssignmentCost>::max(), 0,
      0, std::numeric_limits<AssignmentCost>::max(),
  };
  REQUIRE_EQ(hungarian_min_assignment(2U, 2U, overflow).cost, 0);

  const std::vector<AssignmentCost> hostile{
      std::numeric_limits<AssignmentCost>::min(),
      std::numeric_limits<AssignmentCost>::max(),
  };
  REQUIRE_THROWS_AS(hungarian_min_assignment(1U, 2U, hostile),
                    std::overflow_error);
}

TEST_CASE(hungarian_assignment_randomized_exhaustive_and_flow_cross_check) {
  std::mt19937_64 rng(0xA5516EULL);
  std::uniform_int_distribution<std::size_t> rows_dist(1U, 4U);
  std::uniform_int_distribution<AssignmentCost> cost_dist(-20, 20);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t rows = rows_dist(rng);
    std::uniform_int_distribution<std::size_t> columns_dist(rows, 5U);
    const std::size_t columns = columns_dist(rng);
    std::vector<AssignmentCost> costs(rows * columns);
    for (auto& cost : costs) {
      cost = cost_dist(rng);
    }
    const auto result = hungarian_min_assignment(rows, columns, costs);
    const auto expected = exhaustive_assignment(rows, columns, costs);
    REQUIRE_EQ(result.cost, expected);
    REQUIRE_EQ(replay_cost(rows, columns, costs, result), expected);
    REQUIRE_EQ(min_cost_flow_assignment(rows, columns, costs), expected);
  }
}
