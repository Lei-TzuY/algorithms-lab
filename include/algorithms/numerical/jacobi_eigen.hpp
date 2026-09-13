#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::numerical {

struct JacobiEigenResult {
  std::vector<double> eigenvalues;
  // Column k is the normalized eigenvector for eigenvalues[k].
  std::vector<std::vector<double>> eigenvectors;
  std::size_t sweeps_executed = 0U;
  std::size_t rotations_applied = 0U;
  double maximum_off_diagonal = 0.0;
  bool converged = true;
};

namespace jacobi_eigen_detail {

inline void require_finite(double value, const char* message) {
  if (!std::isfinite(value)) {
    throw std::overflow_error(message);
  }
}

inline std::size_t validate_input(
    const std::vector<std::vector<double>>& matrix, double tolerance,
    std::size_t max_sweeps) {
  if (!std::isfinite(tolerance) || tolerance < 0.0 || tolerance > 1.0) {
    throw std::invalid_argument(
        "Jacobi eigen tolerance must be finite and in [0,1]");
  }
  if (max_sweeps == 0U) {
    throw std::invalid_argument("Jacobi eigen max_sweeps must be positive");
  }
  const std::size_t size = matrix.size();
  for (const auto& row : matrix) {
    if (row.size() != size) {
      throw std::invalid_argument("Jacobi eigen matrix must be square");
    }
    for (const double value : row) {
      if (!std::isfinite(value)) {
        throw std::invalid_argument("Jacobi eigen matrix must be finite");
      }
    }
  }
  for (std::size_t row = 0U; row < size; ++row) {
    for (std::size_t column = row + 1U; column < size; ++column) {
      if (matrix[row][column] != matrix[column][row]) {
        throw std::invalid_argument("Jacobi eigen matrix must be symmetric");
      }
    }
  }
  return size;
}

inline double maximum_off_diagonal(
    const std::vector<std::vector<double>>& matrix) {
  double maximum = 0.0;
  for (std::size_t row = 0U; row < matrix.size(); ++row) {
    for (std::size_t column = row + 1U; column < matrix.size(); ++column) {
      maximum = std::max(maximum, std::abs(matrix[row][column]));
    }
  }
  return maximum;
}

inline double matrix_scale(const std::vector<std::vector<double>>& matrix) {
  double scale = 1.0;
  for (const auto& row : matrix) {
    for (const double value : row) {
      scale = std::max(scale, std::abs(value));
    }
  }
  return scale;
}

inline std::pair<double, double> stable_rotation(double app, double aqq,
                                                 double apq) {
  const double scale = std::max({std::abs(app), std::abs(aqq), std::abs(apq)});
  if (scale == 0.0 || apq == 0.0) {
    return {1.0, 0.0};
  }
  const double normalized_p = app / scale;
  const double normalized_q = aqq / scale;
  const double normalized_off = apq / scale;
  if (normalized_off == 0.0) {
    if (app == aqq) {
      const double inverse_root_two = 1.0 / std::sqrt(2.0);
      return {inverse_root_two, std::copysign(inverse_root_two, apq)};
    }
    return {1.0, 0.0};
  }
  const double tau = (normalized_q - normalized_p) / (2.0 * normalized_off);
  require_finite(tau, "Jacobi eigen rotation parameter overflowed");
  const double sign = tau < 0.0 ? -1.0 : 1.0;
  const double tangent = sign / (std::abs(tau) + std::hypot(1.0, tau));
  require_finite(tangent, "Jacobi eigen rotation parameter overflowed");
  const double cosine = 1.0 / std::hypot(1.0, tangent);
  const double sine = tangent * cosine;
  require_finite(cosine, "Jacobi eigen rotation overflowed");
  require_finite(sine, "Jacobi eigen rotation overflowed");
  return {cosine, sine};
}

inline void canonicalize_eigenvector_signs(
    std::vector<std::vector<double>>& eigenvectors) {
  const std::size_t size = eigenvectors.size();
  for (std::size_t column = 0U; column < size; ++column) {
    std::size_t pivot = 0U;
    double pivot_magnitude = 0.0;
    for (std::size_t row = 0U; row < size; ++row) {
      const double magnitude = std::abs(eigenvectors[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot = row;
      }
    }
    if (pivot_magnitude != 0.0 && eigenvectors[pivot][column] < 0.0) {
      for (std::size_t row = 0U; row < size; ++row) {
        eigenvectors[row][column] = -eigenvectors[row][column];
      }
    }
  }
}

}  // namespace jacobi_eigen_detail

// Cyclic Jacobi eigendecomposition for real symmetric dense matrices.
// The implementation is a numerical baseline: one sweep visits every p<q pair,
// applies an orthogonal plane rotation, and accumulates the same rotations in
// the eigenvector matrix. A finite sweep budget can return converged=false.
inline JacobiEigenResult symmetric_jacobi_eigendecomposition(
    const std::vector<std::vector<double>>& matrix,
    double relative_tolerance = 1e-12, std::size_t max_sweeps = 64U) {
  const std::size_t size = jacobi_eigen_detail::validate_input(
      matrix, relative_tolerance, max_sweeps);

  JacobiEigenResult result;
  result.eigenvectors.assign(size, std::vector<double>(size, 0.0));
  for (std::size_t index = 0U; index < size; ++index) {
    result.eigenvectors[index][index] = 1.0;
  }
  if (size == 0U) {
    return result;
  }

  std::vector<std::vector<double>> work = matrix;
  const double input_scale = jacobi_eigen_detail::matrix_scale(matrix);
  const double threshold = relative_tolerance * input_scale;
  jacobi_eigen_detail::require_finite(
      threshold, "Jacobi eigen convergence threshold overflowed");

  for (std::size_t sweep = 0U; sweep < max_sweeps; ++sweep) {
    const double before = jacobi_eigen_detail::maximum_off_diagonal(work);
    if (before <= threshold) {
      result.converged = true;
      break;
    }

    for (std::size_t p = 0U; p < size; ++p) {
      for (std::size_t q = p + 1U; q < size; ++q) {
        const double apq = work[p][q];
        if (apq == 0.0) {
          continue;
        }
        const auto [cosine, sine] = jacobi_eigen_detail::stable_rotation(
            work[p][p], work[q][q], apq);
        if (sine == 0.0) {
          continue;
        }

        const double app = work[p][p];
        const double aqq = work[q][q];
        const double tangent = sine / cosine;
        jacobi_eigen_detail::require_finite(
            tangent, "Jacobi eigen diagonal update overflowed");
        const double delta = tangent * apq;
        jacobi_eigen_detail::require_finite(
            delta, "Jacobi eigen diagonal update overflowed");
        const double new_app = app - delta;
        const double new_aqq = aqq + delta;
        jacobi_eigen_detail::require_finite(
            new_app, "Jacobi eigen diagonal update overflowed");
        jacobi_eigen_detail::require_finite(
            new_aqq, "Jacobi eigen diagonal update overflowed");

        for (std::size_t k = 0U; k < size; ++k) {
          if (k == p || k == q) {
            continue;
          }
          const double akp = work[k][p];
          const double akq = work[k][q];
          const double new_kp = cosine * akp - sine * akq;
          const double new_kq = sine * akp + cosine * akq;
          jacobi_eigen_detail::require_finite(
              new_kp, "Jacobi eigen matrix update overflowed");
          jacobi_eigen_detail::require_finite(
              new_kq, "Jacobi eigen matrix update overflowed");
          work[k][p] = new_kp;
          work[p][k] = new_kp;
          work[k][q] = new_kq;
          work[q][k] = new_kq;
        }
        work[p][p] = new_app;
        work[q][q] = new_aqq;
        work[p][q] = 0.0;
        work[q][p] = 0.0;

        for (std::size_t row = 0U; row < size; ++row) {
          const double vip = result.eigenvectors[row][p];
          const double viq = result.eigenvectors[row][q];
          const double new_vip = cosine * vip - sine * viq;
          const double new_viq = sine * vip + cosine * viq;
          jacobi_eigen_detail::require_finite(
              new_vip, "Jacobi eigenvector update overflowed");
          jacobi_eigen_detail::require_finite(
              new_viq, "Jacobi eigenvector update overflowed");
          result.eigenvectors[row][p] = new_vip;
          result.eigenvectors[row][q] = new_viq;
        }
        ++result.rotations_applied;
      }
    }
    ++result.sweeps_executed;
    result.maximum_off_diagonal =
        jacobi_eigen_detail::maximum_off_diagonal(work);
    if (result.maximum_off_diagonal <= threshold) {
      result.converged = true;
      break;
    }
    result.converged = false;
  }

  result.maximum_off_diagonal =
      jacobi_eigen_detail::maximum_off_diagonal(work);
  if (result.maximum_off_diagonal <= threshold) {
    result.converged = true;
  }

  std::vector<std::size_t> order(size, 0U);
  std::iota(order.begin(), order.end(), 0U);
  std::stable_sort(order.begin(), order.end(), [&](std::size_t left,
                                                   std::size_t right) {
    return work[left][left] < work[right][right];
  });

  result.eigenvalues.resize(size, 0.0);
  std::vector<std::vector<double>> sorted_vectors(
      size, std::vector<double>(size, 0.0));
  for (std::size_t output = 0U; output < size; ++output) {
    const std::size_t source = order[output];
    result.eigenvalues[output] = work[source][source];
    for (std::size_t row = 0U; row < size; ++row) {
      sorted_vectors[row][output] = result.eigenvectors[row][source];
    }
  }
  result.eigenvectors = std::move(sorted_vectors);
  jacobi_eigen_detail::canonicalize_eigenvector_signs(result.eigenvectors);
  return result;
}

}  // namespace algorithms::numerical
