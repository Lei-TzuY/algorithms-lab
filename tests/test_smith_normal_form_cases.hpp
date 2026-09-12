#pragma once

#include "algorithms/linear_algebra/smith_normal_form.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::linear_algebra::SmithMatrix;
using algorithms::linear_algebra::SmithNormalFormResult;

std::int64_t determinant_recursive(const SmithMatrix& matrix) {
  const std::size_t n = matrix.size();
  REQUIRE(std::all_of(matrix.begin(), matrix.end(),
                      [n](const auto& row) { return row.size() == n; }));
  if (n == 0) return 1;
  if (n == 1) return matrix[0][0];
  std::int64_t result = 0;
  for (std::size_t column = 0; column < n; ++column) {
    SmithMatrix minor(n - 1, std::vector<std::int64_t>(n - 1));
    for (std::size_t row = 1; row < n; ++row) {
      std::size_t out_column = 0;
      for (std::size_t source_column = 0; source_column < n; ++source_column) {
        if (source_column == column) continue;
        minor[row - 1][out_column++] = matrix[row][source_column];
      }
    }
    const std::int64_t term = matrix[0][column] * determinant_recursive(minor);
    result += (column % 2 == 0) ? term : -term;
  }
  return result;
}

SmithMatrix multiply(const SmithMatrix& first, const SmithMatrix& second) {
  const std::size_t rows = first.size();
  const std::size_t shared = rows == 0 ? 0 : first.front().size();
  const std::size_t columns = second.empty() ? 0 : second.front().size();
  REQUIRE(second.size() == shared);
  SmithMatrix result(rows, std::vector<std::int64_t>(columns, 0));
  for (std::size_t i = 0; i < rows; ++i) {
    for (std::size_t k = 0; k < shared; ++k) {
      for (std::size_t j = 0; j < columns; ++j) {
        result[i][j] += first[i][k] * second[k][j];
      }
    }
  }
  return result;
}

std::uint64_t abs_u64(std::int64_t value) {
  if (value >= 0) return static_cast<std::uint64_t>(value);
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

std::uint64_t gcd_u64(std::uint64_t a, std::uint64_t b) {
  while (b != 0) {
    const auto remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

void choose_indices(std::size_t n, std::size_t k, std::size_t start,
                    std::vector<std::size_t>& current,
                    std::vector<std::vector<std::size_t>>& output) {
  if (current.size() == k) {
    output.push_back(current);
    return;
  }
  const std::size_t need = k - current.size();
  for (std::size_t value = start; value + need <= n; ++value) {
    current.push_back(value);
    choose_indices(n, k, value + 1, current, output);
    current.pop_back();
  }
}

std::vector<std::vector<std::size_t>> combinations(std::size_t n,
                                                   std::size_t k) {
  std::vector<std::vector<std::size_t>> result;
  std::vector<std::size_t> current;
  choose_indices(n, k, 0, current, result);
  return result;
}

std::vector<std::int64_t> invariant_factors_from_minors(const SmithMatrix& a) {
  const std::size_t rows = a.size();
  const std::size_t columns = rows == 0 ? 0 : a.front().size();
  std::vector<std::int64_t> factors;
  std::uint64_t previous_delta = 1;
  const std::size_t limit = std::min(rows, columns);
  for (std::size_t size = 1; size <= limit; ++size) {
    std::uint64_t delta = 0;
    const auto row_sets = combinations(rows, size);
    const auto column_sets = combinations(columns, size);
    for (const auto& selected_rows : row_sets) {
      for (const auto& selected_columns : column_sets) {
        SmithMatrix minor(size, std::vector<std::int64_t>(size));
        for (std::size_t i = 0; i < size; ++i) {
          for (std::size_t j = 0; j < size; ++j) {
            minor[i][j] = a[selected_rows[i]][selected_columns[j]];
          }
        }
        delta = gcd_u64(delta, abs_u64(determinant_recursive(minor)));
      }
    }
    if (delta == 0) break;
    REQUIRE(delta % previous_delta == 0);
    const auto factor = delta / previous_delta;
    REQUIRE(factor <= static_cast<std::uint64_t>(
                          std::numeric_limits<std::int64_t>::max()));
    factors.push_back(static_cast<std::int64_t>(factor));
    previous_delta = delta;
  }
  return factors;
}

void verify_result(const SmithMatrix& input, const SmithNormalFormResult& result) {
  const std::size_t rows = input.size();
  const std::size_t columns = rows == 0 ? 0 : input.front().size();
  REQUIRE_EQ(result.left_transform.size(), rows);
  for (const auto& row : result.left_transform) REQUIRE_EQ(row.size(), rows);
  REQUIRE_EQ(result.right_transform.size(), columns);
  for (const auto& row : result.right_transform) REQUIRE_EQ(row.size(), columns);
  REQUIRE_EQ(result.diagonal.size(), rows);
  for (const auto& row : result.diagonal) REQUIRE_EQ(row.size(), columns);
  REQUIRE_EQ(multiply(multiply(result.left_transform, input),
                      result.right_transform),
             result.diagonal);
  REQUIRE_EQ(result.rank, result.invariant_factors.size());

  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t column = 0; column < columns; ++column) {
      if (row != column) REQUIRE_EQ(result.diagonal[row][column], std::int64_t{0});
    }
  }
  for (std::size_t index = 0; index < result.rank; ++index) {
    REQUIRE(result.invariant_factors[index] > 0);
    REQUIRE_EQ(result.diagonal[index][index], result.invariant_factors[index]);
    if (index + 1 < result.rank) {
      REQUIRE(result.invariant_factors[index + 1] %
                  result.invariant_factors[index] ==
              0);
    }
  }
  for (std::size_t index = result.rank; index < std::min(rows, columns);
       ++index) {
    REQUIRE_EQ(result.diagonal[index][index], std::int64_t{0});
  }
  if (rows != 0) {
    const auto det_u = determinant_recursive(result.left_transform);
    REQUIRE(det_u == 1 || det_u == -1);
  }
  if (columns != 0) {
    const auto det_v = determinant_recursive(result.right_transform);
    REQUIRE(det_v == 1 || det_v == -1);
  }
}
}  // namespace

TEST_CASE(smith_normal_form_deterministic_rectangular_and_rank_cases) {
  using algorithms::linear_algebra::smith_normal_form;
  for (const SmithMatrix& matrix : std::vector<SmithMatrix>{
           {},
           {{}, {}},
           {{-6}},
           {{6, 0}, {0, 10}},
           {{2, 4}, {6, 8}},
           {{2, 4, 4}, {6, 6, 12}},
           {{2, 4}, {1, 2}},
           {{0, 0, 0}, {0, 0, 0}},
           {{4, 6}, {3, 9}},
       }) {
    const auto result = smith_normal_form(matrix);
    verify_result(matrix, result);
    REQUIRE_EQ(result.invariant_factors, invariant_factors_from_minors(matrix));
  }
}

TEST_CASE(smith_normal_form_expected_invariant_factors) {
  using algorithms::linear_algebra::smith_normal_form;
  REQUIRE_EQ(smith_normal_form(SmithMatrix{{6, 0}, {0, 10}}).invariant_factors,
             (std::vector<std::int64_t>{2, 30}));
  REQUIRE_EQ(smith_normal_form(SmithMatrix{{2, 4}, {6, 8}}).invariant_factors,
             (std::vector<std::int64_t>{2, 4}));
  REQUIRE_EQ(
      smith_normal_form(SmithMatrix{{2, 4, 4}, {6, 6, 12}}).invariant_factors,
      (std::vector<std::int64_t>{2, 6}));
  REQUIRE_EQ(smith_normal_form(SmithMatrix{{2, 4}, {1, 2}}).invariant_factors,
             (std::vector<std::int64_t>{1}));
}

TEST_CASE(smith_normal_form_validation_and_bounded_overflow) {
  using algorithms::linear_algebra::smith_normal_form;
  REQUIRE_THROWS_AS(smith_normal_form(SmithMatrix{{1, 2}, {3}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      smith_normal_form(
          SmithMatrix{{std::numeric_limits<std::int64_t>::min()}}),
      std::overflow_error);
}

TEST_CASE(smith_normal_form_randomized_differential_against_determinantal_divisors) {
  using algorithms::linear_algebra::smith_normal_form;
  std::mt19937_64 rng(0x534D495448ULL);
  std::uniform_int_distribution<int> dimension(1, 4);
  std::uniform_int_distribution<int> value(-4, 4);
  for (std::size_t trial = 0; trial < 600; ++trial) {
    const auto rows = static_cast<std::size_t>(dimension(rng));
    const auto columns = static_cast<std::size_t>(dimension(rng));
    SmithMatrix matrix(rows, std::vector<std::int64_t>(columns));
    for (auto& row : matrix) {
      for (auto& entry : row) entry = value(rng);
    }
    const auto result = smith_normal_form(matrix);
    verify_result(matrix, result);
    REQUIRE_EQ(result.invariant_factors, invariant_factors_from_minors(matrix));
  }
}
