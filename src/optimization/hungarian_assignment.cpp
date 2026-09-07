#include "algorithms/optimization/hungarian_assignment.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::optimization {
namespace {

[[nodiscard]] AssignmentCost checked_add(AssignmentCost left,
                                         AssignmentCost right,
                                         const char* message) {
  if (right > 0 && left > std::numeric_limits<AssignmentCost>::max() - right) {
    throw std::overflow_error(message);
  }
  if (right < 0 && left < std::numeric_limits<AssignmentCost>::min() - right) {
    throw std::overflow_error(message);
  }
  return left + right;
}

[[nodiscard]] AssignmentCost checked_subtract(AssignmentCost left,
                                              AssignmentCost right,
                                              const char* message) {
  if (right > 0 && left < std::numeric_limits<AssignmentCost>::min() + right) {
    throw std::overflow_error(message);
  }
  if (right < 0 && left > std::numeric_limits<AssignmentCost>::max() + right) {
    throw std::overflow_error(message);
  }
  return left - right;
}

[[nodiscard]] AssignmentCost matrix_at(std::span<const AssignmentCost> matrix,
                                       std::size_t column_count,
                                       std::size_t row,
                                       std::size_t column) {
  return matrix[row * column_count + column];
}

}  // namespace

AssignmentResult hungarian_min_assignment(
    std::size_t row_count, std::size_t column_count,
    std::span<const AssignmentCost> cost_matrix) {
  if (row_count == 0U) {
    if (!cost_matrix.empty()) {
      throw std::invalid_argument("empty assignment row set must have empty cost matrix");
    }
    return AssignmentResult{0, {}};
  }
  if (column_count == 0U || row_count > column_count) {
    throw std::invalid_argument("Hungarian assignment requires 0 < rows <= columns");
  }
  if (row_count > std::numeric_limits<std::size_t>::max() / column_count ||
      cost_matrix.size() != row_count * column_count) {
    throw std::invalid_argument("Hungarian assignment cost matrix shape mismatch");
  }

  // One-indexed textbook state. p[j] is the row currently matched to column j;
  // p[0] is the row being augmented. way[] stores predecessor columns.
  std::vector<AssignmentCost> row_potential(row_count + 1U, 0);
  std::vector<AssignmentCost> column_potential(column_count + 1U, 0);
  std::vector<std::size_t> matched_row(column_count + 1U, 0U);
  std::vector<std::size_t> way(column_count + 1U, 0U);

  for (std::size_t row = 1U; row <= row_count; ++row) {
    matched_row[0] = row;
    std::size_t current_column = 0U;
    std::vector<std::optional<AssignmentCost>> minimum_slack(column_count + 1U);
    std::vector<bool> used(column_count + 1U, false);

    do {
      used[current_column] = true;
      const std::size_t current_row = matched_row[current_column];
      std::optional<AssignmentCost> delta;
      std::size_t next_column = 0U;

      for (std::size_t column = 1U; column <= column_count; ++column) {
        if (used[column]) {
          continue;
        }
        AssignmentCost reduced = matrix_at(cost_matrix, column_count,
                                           current_row - 1U, column - 1U);
        reduced = checked_subtract(
            reduced, row_potential[current_row],
            "Hungarian reduced cost is not representable");
        reduced = checked_subtract(
            reduced, column_potential[column],
            "Hungarian reduced cost is not representable");

        if (!minimum_slack[column].has_value() ||
            reduced < *minimum_slack[column]) {
          minimum_slack[column] = reduced;
          way[column] = current_column;
        }
        if (!delta.has_value() || *minimum_slack[column] < *delta) {
          delta = *minimum_slack[column];
          next_column = column;
        }
      }

      if (!delta.has_value()) {
        throw std::logic_error("Hungarian augmentation found no free column frontier");
      }
      for (std::size_t column = 0U; column <= column_count; ++column) {
        if (used[column]) {
          row_potential[matched_row[column]] = checked_add(
              row_potential[matched_row[column]], *delta,
              "Hungarian row potential is not representable");
          column_potential[column] = checked_subtract(
              column_potential[column], *delta,
              "Hungarian column potential is not representable");
        } else if (minimum_slack[column].has_value()) {
          minimum_slack[column] = checked_subtract(
              *minimum_slack[column], *delta,
              "Hungarian slack is not representable");
        }
      }
      current_column = next_column;
    } while (matched_row[current_column] != 0U);

    do {
      const std::size_t previous_column = way[current_column];
      matched_row[current_column] = matched_row[previous_column];
      current_column = previous_column;
    } while (current_column != 0U);
  }

  std::vector<std::size_t> assignment(row_count, 0U);
  for (std::size_t column = 1U; column <= column_count; ++column) {
    if (matched_row[column] != 0U) {
      assignment[matched_row[column] - 1U] = column - 1U;
    }
  }

  AssignmentCost total = 0;
  for (std::size_t row = 0U; row < row_count; ++row) {
    total = checked_add(total,
                        matrix_at(cost_matrix, column_count, row, assignment[row]),
                        "Hungarian assignment total cost is not representable");
  }
  return AssignmentResult{total, std::move(assignment)};
}

}  // namespace algorithms::optimization
