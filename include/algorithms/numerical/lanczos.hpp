#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::numerical {

enum class LanczosStatus {
  empty,
  invariant_subspace,
  iteration_limit,
};

struct LanczosResult {
  LanczosStatus status = LanczosStatus::empty;
  std::vector<std::vector<double>> basis;
  std::vector<double> diagonal;
  std::vector<double> subdiagonal;
  std::vector<double> residual;
  double residual_norm = 0.0;
  std::size_t steps = 0U;
};

namespace lanczos_detail {

inline void require_finite(const double value, const char* message) {
  if (!std::isfinite(value)) {
    throw std::overflow_error(message);
  }
}

inline double checked_dot(const std::vector<double>& left,
                          const std::vector<double>& right) {
  double sum = 0.0;
  for (std::size_t index = 0U; index < left.size(); ++index) {
    const double product = left[index] * right[index];
    require_finite(product, "Lanczos dot product overflowed");
    sum += product;
    require_finite(sum, "Lanczos dot product overflowed");
  }
  return sum;
}

inline double checked_norm(const std::vector<double>& vector) {
  double result = 0.0;
  for (const double value : vector) {
    result = std::hypot(result, value);
    require_finite(result, "Lanczos vector norm overflowed");
  }
  return result;
}

inline std::vector<double> checked_matvec(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& vector) {
  std::vector<double> result(matrix.size(), 0.0);
  for (std::size_t row = 0U; row < matrix.size(); ++row) {
    double sum = 0.0;
    for (std::size_t column = 0U; column < vector.size(); ++column) {
      const double product = matrix[row][column] * vector[column];
      require_finite(product, "Lanczos matrix product overflowed");
      sum += product;
      require_finite(sum, "Lanczos matrix product overflowed");
    }
    result[row] = sum;
  }
  return result;
}

inline void validate_input(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& starting_vector,
    const std::size_t max_steps,
    const double breakdown_tolerance) {
  const std::size_t size = matrix.size();

  if (!std::isfinite(breakdown_tolerance) ||
      breakdown_tolerance < 0.0) {
    throw std::invalid_argument(
        "Lanczos breakdown tolerance must be finite and non-negative");
  }

  if (size == 0U) {
    if (!starting_vector.empty()) {
      throw std::invalid_argument(
          "Lanczos empty matrix requires an empty starting vector");
    }
    if (max_steps != 0U) {
      throw std::invalid_argument(
          "Lanczos empty matrix requires zero steps");
    }
    return;
  }

  if (starting_vector.size() != size) {
    throw std::invalid_argument(
        "Lanczos starting-vector size mismatch");
  }
  if (max_steps == 0U || max_steps > size) {
    throw std::invalid_argument(
        "Lanczos step limit must lie in [1,n]");
  }

  for (std::size_t row = 0U; row < size; ++row) {
    if (matrix[row].size() != size) {
      throw std::invalid_argument(
          "Lanczos matrix must be square");
    }
    if (!std::isfinite(starting_vector[row])) {
      throw std::invalid_argument(
          "Lanczos starting vector must be finite");
    }
    for (std::size_t column = 0U; column < size; ++column) {
      if (!std::isfinite(matrix[row][column])) {
        throw std::invalid_argument(
            "Lanczos matrix must be finite");
      }
      if (column < row &&
          matrix[row][column] != matrix[column][row]) {
        throw std::invalid_argument(
            "Lanczos matrix must be exactly symmetric");
      }
    }
  }

  if (checked_norm(starting_vector) == 0.0) {
    throw std::invalid_argument(
        "Lanczos starting vector must be non-zero");
  }
}

}  // namespace lanczos_detail

// Classical symmetric Lanczos tridiagonalization.
//
// The returned basis stores q_0,...,q_{k-1}. diagonal[j] is alpha_j and
// subdiagonal[j] is beta_j coupling q_j to q_{j+1}. The terminal residual is
// the unnormalized vector remaining after the final returned basis vector:
//
//   A q_{k-1} = beta_{k-2} q_{k-2} + alpha_{k-1} q_{k-1} + residual.
//
// If residual_norm <= breakdown_tolerance, status is invariant_subspace.
// Otherwise reaching max_steps reports iteration_limit.
//
// This is the classical three-term recurrence. It intentionally does not apply
// full reorthogonalization; finite-precision loss of global orthogonality is a
// numerical boundary of this slice rather than silently changing the recurrence.
inline LanczosResult lanczos_tridiagonalize(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& starting_vector,
    const std::size_t max_steps,
    const double breakdown_tolerance = 0.0) {
  lanczos_detail::validate_input(
      matrix, starting_vector, max_steps, breakdown_tolerance);

  const std::size_t size = matrix.size();
  if (size == 0U) {
    return LanczosResult{};
  }

  const double start_norm =
      lanczos_detail::checked_norm(starting_vector);
  std::vector<double> current(size, 0.0);
  for (std::size_t index = 0U; index < size; ++index) {
    current[index] = starting_vector[index] / start_norm;
    lanczos_detail::require_finite(
        current[index], "Lanczos basis normalization overflowed");
  }

  std::vector<double> previous(size, 0.0);
  double previous_beta = 0.0;

  LanczosResult result;
  result.status = LanczosStatus::iteration_limit;
  result.basis.reserve(max_steps);
  result.diagonal.reserve(max_steps);
  if (max_steps > 0U) {
    result.subdiagonal.reserve(max_steps - 1U);
  }

  for (std::size_t step = 0U; step < max_steps; ++step) {
    result.basis.push_back(current);

    std::vector<double> residual =
        lanczos_detail::checked_matvec(matrix, current);

    if (step != 0U) {
      for (std::size_t index = 0U; index < size; ++index) {
        const double correction = previous_beta * previous[index];
        lanczos_detail::require_finite(
            correction, "Lanczos recurrence overflowed");
        residual[index] -= correction;
        lanczos_detail::require_finite(
            residual[index], "Lanczos recurrence overflowed");
      }
    }

    const double alpha =
        lanczos_detail::checked_dot(current, residual);
    result.diagonal.push_back(alpha);

    for (std::size_t index = 0U; index < size; ++index) {
      const double correction = alpha * current[index];
      lanczos_detail::require_finite(
          correction, "Lanczos recurrence overflowed");
      residual[index] -= correction;
      lanczos_detail::require_finite(
          residual[index], "Lanczos recurrence overflowed");
    }

    const double beta = lanczos_detail::checked_norm(residual);
    result.steps = step + 1U;

    if (beta <= breakdown_tolerance) {
      result.status = LanczosStatus::invariant_subspace;
      result.residual = std::move(residual);
      result.residual_norm = beta;
      return result;
    }

    if (step + 1U == max_steps) {
      result.status = LanczosStatus::iteration_limit;
      result.residual = std::move(residual);
      result.residual_norm = beta;
      return result;
    }

    result.subdiagonal.push_back(beta);
    previous = std::move(current);
    current.assign(size, 0.0);
    for (std::size_t index = 0U; index < size; ++index) {
      current[index] = residual[index] / beta;
      lanczos_detail::require_finite(
          current[index], "Lanczos basis normalization overflowed");
    }
    previous_beta = beta;
  }

  throw std::logic_error("Lanczos iteration terminated unexpectedly");
}

}  // namespace algorithms::numerical
