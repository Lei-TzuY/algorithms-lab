#pragma once

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

enum class ConjugateGradientStatus {
  converged,
  iteration_limit,
  non_positive_curvature,
};

struct ConjugateGradientResult {
  ConjugateGradientStatus status = ConjugateGradientStatus::converged;
  std::vector<double> solution;
  double residual_norm = 0.0;
  std::size_t iterations = 0;
};

namespace detail {

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
    require_finite(product, "conjugate-gradient dot product overflowed");
    sum += product;
    require_finite(sum, "conjugate-gradient dot product overflowed");
  }
  return sum;
}

inline std::vector<double> checked_matvec(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& vector) {
  std::vector<double> result(matrix.size(), 0.0);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    double sum = 0.0;
    for (std::size_t column = 0; column < vector.size(); ++column) {
      const double product = matrix[row][column] * vector[column];
      require_finite(product, "conjugate-gradient matrix product overflowed");
      sum += product;
      require_finite(sum, "conjugate-gradient matrix product overflowed");
    }
    result[row] = sum;
  }
  return result;
}

inline void validate_dense_symmetric_system(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& rhs,
    const std::vector<double>& initial_guess,
    double absolute_tolerance,
    std::size_t max_iterations) {
  const std::size_t size = matrix.size();
  if (rhs.size() != size) {
    throw std::invalid_argument("conjugate-gradient rhs size mismatch");
  }
  if (!initial_guess.empty() && initial_guess.size() != size) {
    throw std::invalid_argument("conjugate-gradient initial-guess size mismatch");
  }
  if (!std::isfinite(absolute_tolerance) || absolute_tolerance <= 0.0) {
    throw std::invalid_argument("conjugate-gradient tolerance must be positive and finite");
  }
  if (size != 0U && max_iterations == 0U) {
    throw std::invalid_argument("conjugate-gradient iteration limit must be positive");
  }

  for (std::size_t row = 0; row < size; ++row) {
    if (matrix[row].size() != size) {
      throw std::invalid_argument("conjugate-gradient matrix must be square");
    }
    if (!std::isfinite(rhs[row])) {
      throw std::invalid_argument("conjugate-gradient rhs must be finite");
    }
    if (!initial_guess.empty() && !std::isfinite(initial_guess[row])) {
      throw std::invalid_argument("conjugate-gradient initial guess must be finite");
    }
    for (std::size_t column = 0; column < size; ++column) {
      if (!std::isfinite(matrix[row][column])) {
        throw std::invalid_argument("conjugate-gradient matrix must be finite");
      }
      if (column < row && matrix[row][column] != matrix[column][row]) {
        throw std::invalid_argument("conjugate-gradient matrix must be exactly symmetric");
      }
    }
  }
}

}  // namespace detail

// Solve A x = b with the first-principles conjugate-gradient recurrence.
//
// Semantic precondition: A is symmetric positive definite. Exact symmetry is
// validated. Positive definiteness is not pre-factored/validated because doing
// so would replace the intended iterative algorithm with an O(n^3) direct
// factorization gate. If the executed Krylov sequence encounters p^T A p <= 0,
// the result reports non_positive_curvature rather than claiming convergence.
//
// The stopping contract is absolute: ||b - A x||_2 <= absolute_tolerance.
// A finite iteration budget can therefore return iteration_limit with the best
// iterate produced by that budget. No finite-precision <= n iteration guarantee
// is claimed.
inline ConjugateGradientResult conjugate_gradient_solve(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& rhs,
    double absolute_tolerance,
    std::size_t max_iterations,
    const std::vector<double>& initial_guess = {}) {
  detail::validate_dense_symmetric_system(matrix, rhs, initial_guess,
                                          absolute_tolerance, max_iterations);
  const std::size_t size = matrix.size();
  if (size == 0U) {
    return ConjugateGradientResult{};
  }

  std::vector<double> solution = initial_guess.empty()
                                     ? std::vector<double>(size, 0.0)
                                     : initial_guess;
  const std::vector<double> ax = detail::checked_matvec(matrix, solution);
  std::vector<double> residual(size, 0.0);
  for (std::size_t index = 0; index < size; ++index) {
    residual[index] = rhs[index] - ax[index];
    detail::require_finite(residual[index],
                           "conjugate-gradient residual overflowed");
  }

  double residual_squared = detail::checked_dot(residual, residual);
  double residual_norm = std::sqrt(residual_squared);
  detail::require_finite(residual_norm,
                         "conjugate-gradient residual norm overflowed");
  if (residual_norm <= absolute_tolerance) {
    return {ConjugateGradientStatus::converged, std::move(solution),
            residual_norm, 0U};
  }

  std::vector<double> direction = residual;
  for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
    const std::vector<double> matrix_direction =
        detail::checked_matvec(matrix, direction);
    const double curvature = detail::checked_dot(direction, matrix_direction);
    if (curvature <= 0.0) {
      return {ConjugateGradientStatus::non_positive_curvature,
              std::move(solution), residual_norm, iteration};
    }

    const double alpha = residual_squared / curvature;
    detail::require_finite(alpha, "conjugate-gradient alpha overflowed");
    for (std::size_t index = 0; index < size; ++index) {
      solution[index] += alpha * direction[index];
      residual[index] -= alpha * matrix_direction[index];
      detail::require_finite(solution[index],
                             "conjugate-gradient solution overflowed");
      detail::require_finite(residual[index],
                             "conjugate-gradient residual overflowed");
    }

    const double next_residual_squared = detail::checked_dot(residual, residual);
    residual_norm = std::sqrt(next_residual_squared);
    detail::require_finite(residual_norm,
                           "conjugate-gradient residual norm overflowed");
    const std::size_t completed_iterations = iteration + 1U;
    if (residual_norm <= absolute_tolerance) {
      return {ConjugateGradientStatus::converged, std::move(solution),
              residual_norm, completed_iterations};
    }

    const double beta = next_residual_squared / residual_squared;
    detail::require_finite(beta, "conjugate-gradient beta overflowed");
    for (std::size_t index = 0; index < size; ++index) {
      direction[index] = residual[index] + beta * direction[index];
      detail::require_finite(direction[index],
                             "conjugate-gradient direction overflowed");
    }
    residual_squared = next_residual_squared;
  }

  return {ConjugateGradientStatus::iteration_limit, std::move(solution),
          residual_norm, max_iterations};
}

}  // namespace algorithms::numerical
