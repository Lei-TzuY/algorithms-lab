#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::numerical {

enum class GmresStatus {
  converged,
  iteration_limit,
  arnoldi_breakdown,
};

struct GmresResult {
  GmresStatus status = GmresStatus::converged;
  std::vector<double> solution;
  double residual_norm = 0.0;
  std::size_t iterations = 0;
  std::size_t krylov_dimension = 0;
};

namespace gmres_detail {

inline void require_finite(double value, const char* what) {
  if (!std::isfinite(value)) {
    throw std::overflow_error(what);
  }
}

inline double checked_dot(const std::vector<double>& left,
                          const std::vector<double>& right) {
  double sum = 0.0;
  for (std::size_t index = 0; index < left.size(); ++index) {
    const double product = left[index] * right[index];
    require_finite(product, "GMRES dot product overflowed");
    sum += product;
    require_finite(sum, "GMRES dot product overflowed");
  }
  return sum;
}

inline double checked_norm(const std::vector<double>& vector) {
  const double squared = checked_dot(vector, vector);
  const double norm = std::sqrt(squared);
  require_finite(norm, "GMRES vector norm overflowed");
  return norm;
}

inline std::vector<double> checked_matvec(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& vector) {
  std::vector<double> result(matrix.size(), 0.0);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    double sum = 0.0;
    for (std::size_t column = 0; column < vector.size(); ++column) {
      const double product = matrix[row][column] * vector[column];
      require_finite(product, "GMRES matrix product overflowed");
      sum += product;
      require_finite(sum, "GMRES matrix product overflowed");
    }
    result[row] = sum;
  }
  return result;
}

inline void validate_system(const std::vector<std::vector<double>>& matrix,
                            const std::vector<double>& rhs,
                            const std::vector<double>& initial_guess,
                            double absolute_tolerance,
                            std::size_t max_iterations) {
  const std::size_t size = matrix.size();
  if (rhs.size() != size) {
    throw std::invalid_argument("GMRES rhs size mismatch");
  }
  if (!initial_guess.empty() && initial_guess.size() != size) {
    throw std::invalid_argument("GMRES initial-guess size mismatch");
  }
  if (!std::isfinite(absolute_tolerance) || absolute_tolerance <= 0.0) {
    throw std::invalid_argument("GMRES tolerance must be positive and finite");
  }
  if (size != 0U && max_iterations == 0U) {
    throw std::invalid_argument("GMRES iteration limit must be positive");
  }
  for (std::size_t row = 0; row < size; ++row) {
    if (matrix[row].size() != size) {
      throw std::invalid_argument("GMRES matrix must be square");
    }
    if (!std::isfinite(rhs[row])) {
      throw std::invalid_argument("GMRES rhs must be finite");
    }
    if (!initial_guess.empty() && !std::isfinite(initial_guess[row])) {
      throw std::invalid_argument("GMRES initial guess must be finite");
    }
    for (double value : matrix[row]) {
      if (!std::isfinite(value)) {
        throw std::invalid_argument("GMRES matrix must be finite");
      }
    }
  }
}

inline std::vector<double> materialize_solution(
    const std::vector<double>& base_solution,
    const std::vector<std::vector<double>>& basis,
    const std::vector<std::vector<double>>& hessenberg,
    const std::vector<double>& transformed_rhs,
    std::size_t dimension) {
  std::vector<double> coefficients(dimension, 0.0);
  for (std::size_t reverse = dimension; reverse > 0U; --reverse) {
    const std::size_t row = reverse - 1U;
    double value = transformed_rhs[row];
    for (std::size_t column = row + 1U; column < dimension; ++column) {
      value -= hessenberg[row][column] * coefficients[column];
      require_finite(value, "GMRES triangular solve overflowed");
    }
    const double diagonal = hessenberg[row][row];
    if (diagonal == 0.0) {
      throw std::runtime_error("GMRES triangular factor is singular");
    }
    coefficients[row] = value / diagonal;
    require_finite(coefficients[row], "GMRES triangular solve overflowed");
  }

  std::vector<double> solution = base_solution;
  for (std::size_t basis_index = 0; basis_index < dimension; ++basis_index) {
    for (std::size_t coordinate = 0; coordinate < solution.size(); ++coordinate) {
      solution[coordinate] += coefficients[basis_index] * basis[basis_index][coordinate];
      require_finite(solution[coordinate], "GMRES solution overflowed");
    }
  }
  return solution;
}

inline double residual_norm(const std::vector<std::vector<double>>& matrix,
                            const std::vector<double>& rhs,
                            const std::vector<double>& solution) {
  const std::vector<double> product = checked_matvec(matrix, solution);
  std::vector<double> residual(rhs.size(), 0.0);
  for (std::size_t index = 0; index < rhs.size(); ++index) {
    residual[index] = rhs[index] - product[index];
    require_finite(residual[index], "GMRES residual overflowed");
  }
  return checked_norm(residual);
}

}  // namespace gmres_detail

// Solve A x = b with unrestarted GMRES using modified Gram-Schmidt Arnoldi
// orthogonalization and incremental Givens rotations.
//
// The matrix is validated only for dense square finite shape. Nonsingularity is
// deliberately not pre-factored. A singular/invariant Krylov path that cannot
// satisfy the residual contract reports arnoldi_breakdown rather than claiming
// convergence.
//
// The stopping contract is absolute: ||b - A x||_2 <= absolute_tolerance,
// measured by an explicit dense residual of the returned iterate. The Arnoldi
// residual estimate is used only to form the least-squares problem, not as the
// public convergence certificate.
//
// This implementation is unrestarted, so at most min(max_iterations, n)
// Arnoldi steps are attempted. No exact-arithmetic <= n-step theorem is claimed
// for IEEE-754 execution.
inline GmresResult gmres_solve(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& rhs,
    double absolute_tolerance,
    std::size_t max_iterations,
    const std::vector<double>& initial_guess = {}) {
  gmres_detail::validate_system(matrix, rhs, initial_guess,
                                absolute_tolerance, max_iterations);
  const std::size_t size = matrix.size();
  if (size == 0U) {
    return GmresResult{};
  }

  std::vector<double> base_solution = initial_guess.empty()
                                          ? std::vector<double>(size, 0.0)
                                          : initial_guess;
  const std::vector<double> initial_product =
      gmres_detail::checked_matvec(matrix, base_solution);
  std::vector<double> initial_residual(size, 0.0);
  for (std::size_t index = 0; index < size; ++index) {
    initial_residual[index] = rhs[index] - initial_product[index];
    gmres_detail::require_finite(initial_residual[index],
                                 "GMRES residual overflowed");
  }
  const double beta = gmres_detail::checked_norm(initial_residual);
  if (beta <= absolute_tolerance) {
    return {GmresStatus::converged, std::move(base_solution), beta, 0U, 0U};
  }

  const std::size_t iteration_limit = std::min(max_iterations, size);
  std::vector<std::vector<double>> basis(
      iteration_limit + 1U, std::vector<double>(size, 0.0));
  for (std::size_t index = 0; index < size; ++index) {
    basis[0][index] = initial_residual[index] / beta;
    gmres_detail::require_finite(basis[0][index],
                                 "GMRES basis normalization overflowed");
  }

  std::vector<std::vector<double>> hessenberg(
      iteration_limit + 1U, std::vector<double>(iteration_limit, 0.0));
  std::vector<double> cosines(iteration_limit, 0.0);
  std::vector<double> sines(iteration_limit, 0.0);
  std::vector<double> transformed_rhs(iteration_limit + 1U, 0.0);
  transformed_rhs[0] = beta;

  std::vector<double> latest_solution = base_solution;
  double latest_residual_norm = beta;

  for (std::size_t column = 0; column < iteration_limit; ++column) {
    std::vector<double> candidate =
        gmres_detail::checked_matvec(matrix, basis[column]);

    for (std::size_t row = 0; row <= column; ++row) {
      const double projection =
          gmres_detail::checked_dot(candidate, basis[row]);
      hessenberg[row][column] = projection;
      for (std::size_t index = 0; index < size; ++index) {
        candidate[index] -= projection * basis[row][index];
        gmres_detail::require_finite(candidate[index],
                                     "GMRES orthogonalization overflowed");
      }
    }

    const double next_norm = gmres_detail::checked_norm(candidate);
    hessenberg[column + 1U][column] = next_norm;
    const bool arnoldi_breakdown = next_norm == 0.0;
    if (!arnoldi_breakdown && column + 1U < basis.size()) {
      for (std::size_t index = 0; index < size; ++index) {
        basis[column + 1U][index] = candidate[index] / next_norm;
        gmres_detail::require_finite(basis[column + 1U][index],
                                     "GMRES basis normalization overflowed");
      }
    }

    for (std::size_t rotation = 0; rotation < column; ++rotation) {
      const double upper = hessenberg[rotation][column];
      const double lower = hessenberg[rotation + 1U][column];
      hessenberg[rotation][column] =
          cosines[rotation] * upper + sines[rotation] * lower;
      hessenberg[rotation + 1U][column] =
          -sines[rotation] * upper + cosines[rotation] * lower;
      gmres_detail::require_finite(hessenberg[rotation][column],
                                   "GMRES Givens rotation overflowed");
      gmres_detail::require_finite(hessenberg[rotation + 1U][column],
                                   "GMRES Givens rotation overflowed");
    }

    const double diagonal = hessenberg[column][column];
    const double subdiagonal = hessenberg[column + 1U][column];
    const double radius = std::hypot(diagonal, subdiagonal);
    gmres_detail::require_finite(radius, "GMRES Givens radius overflowed");
    if (radius == 0.0) {
      return {GmresStatus::arnoldi_breakdown, std::move(latest_solution),
              latest_residual_norm, column + 1U, column};
    }
    cosines[column] = diagonal / radius;
    sines[column] = subdiagonal / radius;
    gmres_detail::require_finite(cosines[column],
                                 "GMRES Givens cosine overflowed");
    gmres_detail::require_finite(sines[column],
                                 "GMRES Givens sine overflowed");

    hessenberg[column][column] = radius;
    hessenberg[column + 1U][column] = 0.0;
    const double current_rhs = transformed_rhs[column];
    const double next_rhs = transformed_rhs[column + 1U];
    transformed_rhs[column] =
        cosines[column] * current_rhs + sines[column] * next_rhs;
    transformed_rhs[column + 1U] =
        -sines[column] * current_rhs + cosines[column] * next_rhs;
    gmres_detail::require_finite(transformed_rhs[column],
                                 "GMRES transformed rhs overflowed");
    gmres_detail::require_finite(transformed_rhs[column + 1U],
                                 "GMRES transformed rhs overflowed");

    const std::size_t dimension = column + 1U;
    latest_solution = gmres_detail::materialize_solution(
        base_solution, basis, hessenberg, transformed_rhs, dimension);
    latest_residual_norm =
        gmres_detail::residual_norm(matrix, rhs, latest_solution);
    if (latest_residual_norm <= absolute_tolerance) {
      return {GmresStatus::converged, std::move(latest_solution),
              latest_residual_norm, dimension, dimension};
    }
    if (arnoldi_breakdown) {
      return {GmresStatus::arnoldi_breakdown, std::move(latest_solution),
              latest_residual_norm, dimension, dimension};
    }
  }

  return {GmresStatus::iteration_limit, std::move(latest_solution),
          latest_residual_norm, iteration_limit, iteration_limit};
}

}  // namespace algorithms::numerical
