#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::combinatorial {

struct ExactCoverSolution {
  std::vector<std::size_t> selected_rows;

  friend bool operator==(const ExactCoverSolution&, const ExactCoverSolution&) = default;
};

// Solve the exact-cover problem over columns [0, column_count).
// Each input row lists the columns covered by that row. A solution selects rows
// so every column is covered exactly once. Duplicate column indices inside one
// row and out-of-range column indices are rejected.
//
// Determinism: the solver chooses the active column with minimum cardinality,
// breaking ties by column index, and visits candidate rows in input order.
[[nodiscard]] std::optional<ExactCoverSolution> solve_exact_cover(
    std::size_t column_count,
    const std::vector<std::vector<std::size_t>>& rows);

}  // namespace algorithms::combinatorial
