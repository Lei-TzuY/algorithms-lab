#pragma once

#include "algorithms/numerical/singular_value_decomposition.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::streaming {

class FrequentDirectionsSketch {
 public:
  FrequentDirectionsSketch(std::size_t sketch_rows, std::size_t dimensions,
                           double svd_tolerance = 1.0e-12,
                           std::size_t svd_max_sweeps = 200U)
      : sketch_rows_(sketch_rows),
        dimensions_(dimensions),
        svd_tolerance_(svd_tolerance),
        svd_max_sweeps_(svd_max_sweeps),
        sketch_(sketch_rows, std::vector<double>(dimensions, 0.0)) {
    if (sketch_rows_ == 0U)
      throw std::invalid_argument("Frequent Directions requires sketch_rows > 0");
    if (dimensions_ == 0U)
      throw std::invalid_argument("Frequent Directions requires dimensions > 0");
    if (sketch_rows_ > dimensions_)
      throw std::invalid_argument(
          "Frequent Directions requires sketch_rows <= dimensions");
    if (!std::isfinite(svd_tolerance_) || !(svd_tolerance_ > 0.0))
      throw std::invalid_argument(
          "Frequent Directions SVD tolerance must be finite and positive");
    if (svd_max_sweeps_ == 0U)
      throw std::invalid_argument(
          "Frequent Directions SVD max_sweeps must be positive");
  }

  void append(std::span<const double> row) {
    if (row.size() != dimensions_)
      throw std::invalid_argument("Frequent Directions row dimension mismatch");

    std::vector<double> safe_row(row.begin(), row.end());
    long double row_norm_squared = 0.0L;
    for (double value : safe_row) {
      if (!std::isfinite(value))
        throw std::invalid_argument("Frequent Directions row must be finite");
      const long double widened = static_cast<long double>(value);
      row_norm_squared += widened * widened;
      if (!std::isfinite(row_norm_squared))
        throw std::overflow_error("Frequent Directions row norm overflowed");
    }
    if (rows_seen_ == std::numeric_limits<std::size_t>::max())
      throw std::overflow_error("Frequent Directions row count overflowed");
    const long double new_input_energy = input_frobenius_squared_ + row_norm_squared;
    if (!std::isfinite(new_input_energy))
      throw std::overflow_error("Frequent Directions input energy overflowed");

    if (next_free_row_ == sketch_rows_) compress_full_sketch();

    sketch_[next_free_row_] = std::move(safe_row);
    ++next_free_row_;
    ++rows_seen_;
    input_frobenius_squared_ = new_input_energy;
  }

  [[nodiscard]] std::size_t sketch_rows() const noexcept { return sketch_rows_; }
  [[nodiscard]] std::size_t dimensions() const noexcept { return dimensions_; }
  [[nodiscard]] std::size_t rows_seen() const noexcept { return rows_seen_; }
  [[nodiscard]] std::size_t compression_count() const noexcept {
    return compression_count_;
  }
  [[nodiscard]] std::size_t occupied_rows() const noexcept {
    return next_free_row_;
  }
  [[nodiscard]] const std::vector<std::vector<double>>& sketch() const noexcept {
    return sketch_;
  }
  [[nodiscard]] long double input_frobenius_squared() const noexcept {
    return input_frobenius_squared_;
  }
  [[nodiscard]] long double cumulative_shrink_delta() const noexcept {
    return cumulative_shrink_delta_;
  }

  [[nodiscard]] long double sketch_frobenius_squared() const noexcept {
    long double total = 0.0L;
    for (const auto& row : sketch_)
      for (double value : row) {
        const long double widened = static_cast<long double>(value);
        total += widened * widened;
      }
    return total;
  }

  [[nodiscard]] bool valid_state() const noexcept {
    if (sketch_rows_ == 0U || dimensions_ == 0U ||
        sketch_rows_ > dimensions_ || next_free_row_ > sketch_rows_)
      return false;
    if (sketch_.size() != sketch_rows_) return false;
    for (const auto& row : sketch_) {
      if (row.size() != dimensions_) return false;
      for (double value : row)
        if (!std::isfinite(value)) return false;
    }
    if (!std::isfinite(input_frobenius_squared_) ||
        !std::isfinite(cumulative_shrink_delta_))
      return false;
    return true;
  }

 private:
  void compress_full_sketch() {
    const auto svd = algorithms::numerical::one_sided_jacobi_svd(
        sketch_, svd_tolerance_, svd_max_sweeps_);
    if (svd.singular_values.size() != sketch_rows_ ||
        svd.v.size() != dimensions_)
      throw std::runtime_error("Frequent Directions SVD shape mismatch");
    for (const auto& row : svd.v)
      if (row.size() != sketch_rows_)
        throw std::runtime_error("Frequent Directions SVD shape mismatch");

    const long double smallest =
        static_cast<long double>(svd.singular_values.back());
    const long double delta = smallest * smallest;
    if (!std::isfinite(delta))
      throw std::overflow_error("Frequent Directions shrink delta overflowed");

    std::vector<std::vector<double>> rebuilt(
        sketch_rows_, std::vector<double>(dimensions_, 0.0));
    for (std::size_t i = 0U; i < sketch_rows_; ++i) {
      const long double sigma =
          static_cast<long double>(svd.singular_values[i]);
      const long double squared = sigma * sigma;
      const long double shrunk_squared = std::max(0.0L, squared - delta);
      const double shrunk = static_cast<double>(std::sqrt(shrunk_squared));
      if (!std::isfinite(shrunk))
        throw std::overflow_error("Frequent Directions shrink overflowed");
      for (std::size_t column = 0U; column < dimensions_; ++column) {
        const double value = shrunk * svd.v[column][i];
        if (!std::isfinite(value))
          throw std::overflow_error("Frequent Directions rebuild overflowed");
        rebuilt[i][column] = value;
      }
    }

    const long double new_cumulative = cumulative_shrink_delta_ + delta;
    if (!std::isfinite(new_cumulative))
      throw std::overflow_error(
          "Frequent Directions cumulative shrink delta overflowed");

    sketch_ = std::move(rebuilt);
    cumulative_shrink_delta_ = new_cumulative;
    ++compression_count_;
    next_free_row_ = sketch_rows_ - 1U;
  }

  std::size_t sketch_rows_;
  std::size_t dimensions_;
  double svd_tolerance_;
  std::size_t svd_max_sweeps_;
  std::vector<std::vector<double>> sketch_;
  std::size_t next_free_row_ = 0U;
  std::size_t rows_seen_ = 0U;
  std::size_t compression_count_ = 0U;
  long double input_frobenius_squared_ = 0.0L;
  long double cumulative_shrink_delta_ = 0.0L;
};

}  // namespace algorithms::streaming
