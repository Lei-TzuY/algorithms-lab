#pragma once

#include "algorithms/numerical/svd_jacobi_detail.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::numerical {

struct SingularValueDecompositionResult {
  std::vector<std::vector<double>> u;
  std::vector<double> singular_values;
  std::vector<std::vector<double>> v;
  std::size_t sweeps = 0U;
};

// Thin numerical SVD A ~= U diag(s) V^T, r=min(rows, columns).
// Values below the tolerance-scaled numerical-rank threshold are reported as
// zero. This is a first-principles one-sided Jacobi baseline, not a claim of
// LAPACK-grade backward stability.
inline SingularValueDecompositionResult one_sided_jacobi_svd(
    const std::vector<std::vector<double>>& matrix, double tolerance = 1.0e-12,
    std::size_t max_sweeps = 100U) {
  if (!std::isfinite(tolerance) || !(tolerance > 0.0))
    throw std::invalid_argument("SVD tolerance must be finite and positive");
  if (max_sweeps == 0U)
    throw std::invalid_argument("SVD max_sweeps must be positive");

  const std::size_t rows = matrix.size();
  const std::size_t columns = svd_detail::validate_matrix(matrix);
  if (rows == 0U || columns == 0U)
    return {std::vector<std::vector<double>>(rows), {},
            std::vector<std::vector<double>>(columns), 0U};

  if (rows >= columns) {
    auto result = svd_detail::decompose_tall(matrix, tolerance, max_sweeps);
    return {std::move(result.u), std::move(result.values), std::move(result.v),
            result.sweeps};
  }
  auto result = svd_detail::decompose_tall(
      svd_detail::transpose(matrix, columns), tolerance, max_sweeps);
  return {std::move(result.v), std::move(result.values), std::move(result.u),
          result.sweeps};
}

}  // namespace algorithms::numerical
