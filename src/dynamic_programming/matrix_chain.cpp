#include "algorithms/dynamic_programming/matrix_chain.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::dynamic_programming {
namespace {

bool checked_multiply(std::uint64_t left, std::uint64_t right,
                      std::uint64_t& product) {
  if (left != 0U &&
      right > std::numeric_limits<std::uint64_t>::max() / left) {
    return false;
  }
  product = left * right;
  return true;
}

bool checked_add(std::uint64_t left, std::uint64_t right,
                 std::uint64_t& sum) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    return false;
  }
  sum = left + right;
  return true;
}

bool multiplication_cost(std::uint64_t rows, std::uint64_t inner,
                         std::uint64_t columns, std::uint64_t& cost) {
  std::uint64_t partial = 0;
  return checked_multiply(rows, inner, partial) &&
         checked_multiply(partial, columns, cost);
}

std::size_t table_index(std::size_t first, std::size_t last,
                        std::size_t matrix_count) {
  return first * matrix_count + last;
}

void append_split_plan(std::size_t first, std::size_t last,
                       std::size_t matrix_count,
                       const std::vector<std::size_t>& split_table,
                       std::vector<MatrixChainSplit>& output) {
  if (first >= last) {
    return;
  }

  const std::size_t split =
      split_table[table_index(first, last, matrix_count)];
  output.push_back(MatrixChainSplit{first, last, split});
  append_split_plan(first, split, matrix_count, split_table, output);
  append_split_plan(split + 1U, last, matrix_count, split_table, output);
}

}  // namespace

MatrixChainResult matrix_chain_order(
    const std::vector<std::uint64_t>& dimensions) {
  for (const std::uint64_t dimension : dimensions) {
    if (dimension == 0U) {
      throw std::invalid_argument("matrix dimensions must be positive");
    }
  }

  if (dimensions.size() <= 1U) {
    return MatrixChainResult{};
  }

  const std::size_t matrix_count = dimensions.size() - 1U;
  if (matrix_count == 1U) {
    return MatrixChainResult{};
  }

  if (matrix_count >
      std::numeric_limits<std::size_t>::max() / matrix_count) {
    throw std::length_error("matrix-chain DP table dimensions overflow");
  }
  const std::size_t table_size = matrix_count * matrix_count;

  std::vector<std::optional<std::uint64_t>> costs(table_size);
  std::vector<std::size_t> splits(table_size, 0U);
  for (std::size_t index = 0; index < matrix_count; ++index) {
    costs[table_index(index, index, matrix_count)] = std::uint64_t{0};
  }

  for (std::size_t length = 2U; length <= matrix_count; ++length) {
    for (std::size_t first = 0; first + length <= matrix_count; ++first) {
      const std::size_t last = first + length - 1U;
      std::optional<std::uint64_t> best_cost;
      std::size_t best_split = first;

      for (std::size_t split = first; split < last; ++split) {
        const auto& left = costs[table_index(first, split, matrix_count)];
        const auto& right =
            costs[table_index(split + 1U, last, matrix_count)];
        if (!left.has_value() || !right.has_value()) {
          continue;
        }

        std::uint64_t multiply = 0;
        if (!multiplication_cost(dimensions[first], dimensions[split + 1U],
                                 dimensions[last + 1U], multiply)) {
          continue;
        }

        std::uint64_t partial = 0;
        std::uint64_t candidate = 0;
        if (!checked_add(*left, *right, partial) ||
            !checked_add(partial, multiply, candidate)) {
          continue;
        }

        if (!best_cost.has_value() || candidate < *best_cost) {
          best_cost = candidate;
          best_split = split;
        }
      }

      if (best_cost.has_value()) {
        costs[table_index(first, last, matrix_count)] = best_cost;
        splits[table_index(first, last, matrix_count)] = best_split;
      }
    }
  }

  const auto& final_cost =
      costs[table_index(0U, matrix_count - 1U, matrix_count)];
  if (!final_cost.has_value()) {
    throw std::overflow_error(
        "minimum matrix-chain multiplication cost is not representable");
  }

  MatrixChainResult result;
  result.minimum_scalar_multiplications = *final_cost;
  result.splits.reserve(matrix_count - 1U);
  append_split_plan(0U, matrix_count - 1U, matrix_count, splits,
                    result.splits);
  return result;
}

}  // namespace algorithms::dynamic_programming
