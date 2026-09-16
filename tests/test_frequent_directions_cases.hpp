#pragma once

#include "algorithms/streaming/frequent_directions.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace frequent_directions_test_detail {
using Matrix = std::vector<std::vector<double>>;

inline bool close(double a, double b, double scale = 1.0) {
  return std::abs(a - b) <= 2.0e-7 * std::max({scale, std::abs(a), std::abs(b)});
}
inline bool close_long(long double a, long double b, long double scale = 1.0L) {
  return std::abs(a - b) <= 5.0e-7L * std::max({scale, std::abs(a), std::abs(b)});
}
inline Matrix covariance(const Matrix& rows, std::size_t dimensions) {
  Matrix result(dimensions, std::vector<double>(dimensions, 0.0));
  for (const auto& row : rows)
    for (std::size_t i = 0U; i < dimensions; ++i)
      for (std::size_t j = 0U; j < dimensions; ++j)
        result[i][j] += row[i] * row[j];
  return result;
}
inline Matrix subtract(const Matrix& a, const Matrix& b) {
  Matrix result = a;
  for (std::size_t i = 0U; i < a.size(); ++i)
    for (std::size_t j = 0U; j < a.size(); ++j) result[i][j] -= b[i][j];
  return result;
}
inline double quadratic(const Matrix& matrix, const std::vector<double>& x) {
  double result = 0.0;
  for (std::size_t i = 0U; i < x.size(); ++i)
    for (std::size_t j = 0U; j < x.size(); ++j)
      result += x[i] * matrix[i][j] * x[j];
  return result;
}
inline std::vector<double> symmetric_eigenvalues(Matrix matrix) {
  const std::size_t n = matrix.size();
  for (std::size_t sweep = 0U; sweep < 200U; ++sweep) {
    double largest = 0.0;
    std::size_t p = 0U, q = 0U;
    for (std::size_t i = 0U; i < n; ++i)
      for (std::size_t j = i + 1U; j < n; ++j)
        if (std::abs(matrix[i][j]) > largest) {
          largest = std::abs(matrix[i][j]); p = i; q = j;
        }
    if (largest <= 1.0e-12) break;
    const double app = matrix[p][p], aqq = matrix[q][q], apq = matrix[p][q];
    const double angle = 0.5 * std::atan2(2.0 * apq, aqq - app);
    const double c = std::cos(angle), s = std::sin(angle);
    for (std::size_t k = 0U; k < n; ++k) {
      if (k == p || k == q) continue;
      const double mkp = matrix[k][p], mkq = matrix[k][q];
      matrix[k][p] = matrix[p][k] = c * mkp - s * mkq;
      matrix[k][q] = matrix[q][k] = s * mkp + c * mkq;
    }
    matrix[p][p] = c * c * app - 2.0 * s * c * apq + s * s * aqq;
    matrix[q][q] = s * s * app + 2.0 * s * c * apq + c * c * aqq;
    matrix[p][q] = matrix[q][p] = 0.0;
  }
  std::vector<double> values(n, 0.0);
  for (std::size_t i = 0U; i < n; ++i) values[i] = matrix[i][i];
  std::sort(values.begin(), values.end(), std::greater<double>());
  return values;
}
inline long double frobenius_squared(const Matrix& rows) {
  long double total = 0.0L;
  for (const auto& row : rows) for (double value : row) {
    const long double x = static_cast<long double>(value); total += x * x;
  }
  return total;
}
}  // namespace frequent_directions_test_detail

TEST_CASE(frequent_directions_validation_and_transactional_rejection) {
  using algorithms::streaming::FrequentDirectionsSketch;
  REQUIRE_THROWS_AS(FrequentDirectionsSketch(0U, 3U), std::invalid_argument);
  REQUIRE_THROWS_AS(FrequentDirectionsSketch(4U, 3U), std::invalid_argument);
  FrequentDirectionsSketch sketch(2U, 3U);
  sketch.append(std::vector<double>{1.0, 2.0, 3.0});
  const auto before = sketch.sketch();
  const auto rows = sketch.rows_seen();
  REQUIRE_THROWS_AS(sketch.append(std::vector<double>{1.0, 2.0}), std::invalid_argument);
  REQUIRE_THROWS_AS(sketch.append(std::vector<double>{1.0, std::numeric_limits<double>::infinity(), 3.0}), std::invalid_argument);
  REQUIRE_EQ(sketch.rows_seen(), rows);
  REQUIRE(sketch.sketch() == before);
  REQUIRE(sketch.valid_state());
}

TEST_CASE(frequent_directions_known_shrink_and_loss_identity) {
  using algorithms::streaming::FrequentDirectionsSketch;
  using namespace frequent_directions_test_detail;
  FrequentDirectionsSketch sketch(2U, 2U);
  sketch.append(std::vector<double>{3.0, 0.0});
  sketch.append(std::vector<double>{0.0, 2.0});
  REQUIRE_EQ(sketch.compression_count(), 0U);
  sketch.append(std::vector<double>{0.0, 1.0});
  REQUIRE_EQ(sketch.compression_count(), 1U);
  REQUIRE(close_long(sketch.cumulative_shrink_delta(), 4.0L));
  const auto cov = covariance(sketch.sketch(), 2U);
  REQUIRE(close(cov[0][0], 5.0));
  REQUIRE(close(cov[1][1], 1.0));
  REQUIRE(close(cov[0][1], 0.0));
  REQUIRE(close_long(sketch.input_frobenius_squared() - sketch.sketch_frobenius_squared(), 8.0L));
  REQUIRE(close_long(sketch.input_frobenius_squared() - sketch.sketch_frobenius_squared(),
                     2.0L * sketch.cumulative_shrink_delta()));
}

TEST_CASE(frequent_directions_rank_deficient_compression_and_replay) {
  using algorithms::streaming::FrequentDirectionsSketch;
  FrequentDirectionsSketch first(2U, 3U), second(2U, 3U);
  const std::vector<std::vector<double>> rows{{1.0,2.0,3.0},{2.0,4.0,6.0},{-1.0,0.0,1.0},{3.0,1.0,-2.0}};
  for (const auto& row : rows) { first.append(row); second.append(row); }
  REQUIRE(first.valid_state());
  REQUIRE(second.valid_state());
  REQUIRE(first.sketch() == second.sketch());
  REQUIRE_EQ(first.compression_count(), second.compression_count());
  REQUIRE(frequent_directions_test_detail::close_long(
      first.input_frobenius_squared() - first.sketch_frobenius_squared(),
      static_cast<long double>(first.sketch_rows()) * first.cumulative_shrink_delta(), 10.0L));
}

TEST_CASE(frequent_directions_randomized_covariance_and_tail_bound) {
  using algorithms::streaming::FrequentDirectionsSketch;
  using namespace frequent_directions_test_detail;
  std::mt19937_64 rng(0xFDE1C710ULL);
  std::uniform_int_distribution<int> dimensions_dist(2, 6);
  std::uniform_int_distribution<int> value_dist(-4, 4);
  for (std::size_t trial = 0U; trial < 260U; ++trial) {
    const std::size_t d = static_cast<std::size_t>(dimensions_dist(rng));
    std::uniform_int_distribution<int> ell_dist(1, static_cast<int>(d));
    const std::size_t ell = static_cast<std::size_t>(ell_dist(rng));
    std::uniform_int_distribution<int> row_count_dist(static_cast<int>(ell), 28);
    const std::size_t n = static_cast<std::size_t>(row_count_dist(rng));
    FrequentDirectionsSketch sketch(ell, d, 1.0e-12, 300U);
    Matrix input;
    input.reserve(n);
    for (std::size_t r = 0U; r < n; ++r) {
      std::vector<double> row(d, 0.0);
      for (double& value : row) value = static_cast<double>(value_dist(rng));
      input.push_back(row);
      sketch.append(row);
    }
    REQUIRE(sketch.valid_state());
    const Matrix input_cov = covariance(input, d);
    const Matrix sketch_cov = covariance(sketch.sketch(), d);
    const Matrix loss = subtract(input_cov, sketch_cov);
    const auto loss_eigen = symmetric_eigenvalues(loss);
    const auto input_eigen = symmetric_eigenvalues(input_cov);
    REQUIRE(loss_eigen.back() >= -2.0e-5);
    const double max_loss = std::max(0.0, loss_eigen.front());
    for (std::size_t k = 0U; k < ell; ++k) {
      double tail = 0.0;
      for (std::size_t i = k; i < input_eigen.size(); ++i)
        tail += std::max(0.0, input_eigen[i]);
      const double bound = tail / static_cast<double>(ell - k);
      REQUIRE(max_loss <= bound + 2.0e-4 * std::max(1.0, bound));
    }
    REQUIRE(close_long(sketch.input_frobenius_squared(), frobenius_squared(input), 100.0L));
    REQUIRE(close_long(sketch.input_frobenius_squared() - sketch.sketch_frobenius_squared(),
                       static_cast<long double>(ell) * sketch.cumulative_shrink_delta(), 100.0L));
    for (std::size_t probe = 0U; probe < 12U; ++probe) {
      std::vector<double> x(d, 0.0);
      double norm = 0.0;
      for (double& value : x) { value = static_cast<double>(value_dist(rng)); norm += value * value; }
      if (norm == 0.0) { x[0] = 1.0; norm = 1.0; }
      norm = std::sqrt(norm);
      for (double& value : x) value /= norm;
      REQUIRE(quadratic(loss, x) >= -2.0e-5);
    }
  }
}
