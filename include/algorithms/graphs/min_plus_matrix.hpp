#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

class MinPlusMatrix {
 public:
  using Value = std::optional<Weight>;

  MinPlusMatrix(std::size_t rows, std::size_t columns)
      : rows_(rows), columns_(columns), values_(checked_size(rows, columns)) {}

  MinPlusMatrix(std::size_t rows, std::size_t columns,
                std::vector<Value> values)
      : rows_(rows), columns_(columns), values_(std::move(values)) {
    const std::size_t expected = checked_size(rows, columns);
    if (values_.size() != expected) {
      throw std::invalid_argument("min-plus matrix value count mismatch");
    }
  }

  [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
  [[nodiscard]] std::size_t columns() const noexcept { return columns_; }

  [[nodiscard]] const Value& at(std::size_t row, std::size_t column) const {
    return values_.at(index(row, column));
  }

  Value& at(std::size_t row, std::size_t column) {
    return values_.at(index(row, column));
  }

  [[nodiscard]] const std::vector<Value>& values() const noexcept {
    return values_;
  }

  friend bool operator==(const MinPlusMatrix&, const MinPlusMatrix&) = default;

 private:
  static std::size_t checked_size(std::size_t rows, std::size_t columns) {
    if (rows != 0U && columns > std::numeric_limits<std::size_t>::max() / rows) {
      throw std::length_error("min-plus matrix dimensions overflow");
    }
    return rows * columns;
  }

  [[nodiscard]] std::size_t index(std::size_t row, std::size_t column) const {
    if (row >= rows_ || column >= columns_) {
      throw std::out_of_range("min-plus matrix index out of range");
    }
    return row * columns_ + column;
  }

  std::size_t rows_{};
  std::size_t columns_{};
  std::vector<Value> values_;
};

namespace min_plus_detail {

enum class SumClass { representable, below_minimum, above_maximum };

struct ClassifiedSum {
  SumClass kind{SumClass::representable};
  Weight value{};
};

[[nodiscard]] inline ClassifiedSum classify_sum(Weight first, Weight second) {
  constexpr Weight maximum = std::numeric_limits<Weight>::max();
  constexpr Weight minimum = std::numeric_limits<Weight>::min();

  if (second > 0 && first > maximum - second) {
    return {SumClass::above_maximum, 0};
  }
  if (second < 0 && first < minimum - second) {
    return {SumClass::below_minimum, 0};
  }
  return {SumClass::representable, static_cast<Weight>(first + second)};
}

inline void validate_nonnegative(const MinPlusMatrix& matrix) {
  for (const auto& value : matrix.values()) {
    if (value.has_value() && *value < 0) {
      throw std::invalid_argument(
          "min-plus power requires non-negative finite entries");
    }
  }
}

}  // namespace min_plus_detail

[[nodiscard]] inline MinPlusMatrix min_plus_identity(std::size_t dimension) {
  MinPlusMatrix result(dimension, dimension);
  for (std::size_t index = 0; index < dimension; ++index) {
    result.at(index, index) = Weight{0};
  }
  return result;
}

[[nodiscard]] inline MinPlusMatrix min_plus_product(const MinPlusMatrix& first,
                                                    const MinPlusMatrix& second) {
  if (first.columns() != second.rows()) {
    throw std::invalid_argument("min-plus matrix dimensions are incompatible");
  }

  MinPlusMatrix result(first.rows(), second.columns());
  for (std::size_t row = 0; row < first.rows(); ++row) {
    for (std::size_t column = 0; column < second.columns(); ++column) {
      std::optional<Weight> best;
      bool saw_above_maximum = false;
      bool saw_below_minimum = false;

      for (std::size_t inner = 0; inner < first.columns(); ++inner) {
        const auto& left = first.at(row, inner);
        const auto& right = second.at(inner, column);
        if (!left.has_value() || !right.has_value()) {
          continue;
        }

        const auto candidate = min_plus_detail::classify_sum(*left, *right);
        if (candidate.kind == min_plus_detail::SumClass::below_minimum) {
          saw_below_minimum = true;
          continue;
        }
        if (candidate.kind == min_plus_detail::SumClass::above_maximum) {
          saw_above_maximum = true;
          continue;
        }
        if (!best.has_value() || candidate.value < *best) {
          best = candidate.value;
        }
      }

      if (saw_below_minimum || (!best.has_value() && saw_above_maximum)) {
        throw std::overflow_error(
            "min-plus product has an unrepresentable finite minimum");
      }
      result.at(row, column) = best;
    }
  }
  return result;
}

namespace min_plus_detail {

enum class NonnegativeKind { infinity, representable, above_maximum };

struct NonnegativeValue {
  NonnegativeKind kind{NonnegativeKind::infinity};
  Weight value{};
};

class NonnegativeMatrix {
 public:
  explicit NonnegativeMatrix(std::size_t dimension)
      : dimension_(dimension), values_(checked_size(dimension)) {}

  [[nodiscard]] std::size_t dimension() const noexcept { return dimension_; }

  NonnegativeValue& at(std::size_t row, std::size_t column) {
    return values_.at(row * dimension_ + column);
  }

  [[nodiscard]] const NonnegativeValue& at(std::size_t row,
                                            std::size_t column) const {
    return values_.at(row * dimension_ + column);
  }

 private:
  static std::size_t checked_size(std::size_t dimension) {
    if (dimension != 0U &&
        dimension > std::numeric_limits<std::size_t>::max() / dimension) {
      throw std::length_error("min-plus power dimensions overflow");
    }
    return dimension * dimension;
  }

  std::size_t dimension_{};
  std::vector<NonnegativeValue> values_;
};

[[nodiscard]] inline NonnegativeMatrix widen_nonnegative(
    const MinPlusMatrix& matrix) {
  NonnegativeMatrix result(matrix.rows());
  for (std::size_t row = 0; row < matrix.rows(); ++row) {
    for (std::size_t column = 0; column < matrix.columns(); ++column) {
      const auto& value = matrix.at(row, column);
      if (value.has_value()) {
        result.at(row, column) =
            {NonnegativeKind::representable, *value};
      }
    }
  }
  return result;
}

[[nodiscard]] inline NonnegativeMatrix nonnegative_identity(
    std::size_t dimension) {
  NonnegativeMatrix result(dimension);
  for (std::size_t index = 0; index < dimension; ++index) {
    result.at(index, index) = {NonnegativeKind::representable, Weight{0}};
  }
  return result;
}

[[nodiscard]] inline NonnegativeMatrix nonnegative_product(
    const NonnegativeMatrix& first, const NonnegativeMatrix& second) {
  const std::size_t dimension = first.dimension();
  NonnegativeMatrix result(dimension);
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t column = 0; column < dimension; ++column) {
      std::optional<Weight> best;
      bool saw_above = false;
      for (std::size_t inner = 0; inner < dimension; ++inner) {
        const NonnegativeValue& left = first.at(row, inner);
        const NonnegativeValue& right = second.at(inner, column);
        if (left.kind == NonnegativeKind::infinity ||
            right.kind == NonnegativeKind::infinity) {
          continue;
        }
        if (left.kind == NonnegativeKind::above_maximum ||
            right.kind == NonnegativeKind::above_maximum) {
          saw_above = true;
          continue;
        }
        const ClassifiedSum candidate = classify_sum(left.value, right.value);
        if (candidate.kind == SumClass::above_maximum) {
          saw_above = true;
          continue;
        }
        if (candidate.kind == SumClass::below_minimum) {
          throw std::logic_error(
              "non-negative min-plus multiplication produced negative overflow");
        }
        if (!best.has_value() || candidate.value < *best) {
          best = candidate.value;
        }
      }
      if (best.has_value()) {
        result.at(row, column) = {NonnegativeKind::representable, *best};
      } else if (saw_above) {
        result.at(row, column) = {NonnegativeKind::above_maximum, Weight{0}};
      }
    }
  }
  return result;
}

[[nodiscard]] inline MinPlusMatrix narrow_nonnegative(
    const NonnegativeMatrix& matrix) {
  MinPlusMatrix result(matrix.dimension(), matrix.dimension());
  for (std::size_t row = 0; row < matrix.dimension(); ++row) {
    for (std::size_t column = 0; column < matrix.dimension(); ++column) {
      const NonnegativeValue& value = matrix.at(row, column);
      if (value.kind == NonnegativeKind::above_maximum) {
        throw std::overflow_error(
            "min-plus power has an unrepresentable finite result");
      }
      if (value.kind == NonnegativeKind::representable) {
        result.at(row, column) = value.value;
      }
    }
  }
  return result;
}

}  // namespace min_plus_detail

[[nodiscard]] inline MinPlusMatrix min_plus_power_nonnegative(
    const MinPlusMatrix& matrix, std::uint64_t exponent) {
  if (matrix.rows() != matrix.columns()) {
    throw std::invalid_argument("min-plus power requires a square matrix");
  }
  min_plus_detail::validate_nonnegative(matrix);

  min_plus_detail::NonnegativeMatrix result =
      min_plus_detail::nonnegative_identity(matrix.rows());
  min_plus_detail::NonnegativeMatrix factor =
      min_plus_detail::widen_nonnegative(matrix);
  std::uint64_t remaining = exponent;
  while (remaining != 0U) {
    if ((remaining & 1U) != 0U) {
      result = min_plus_detail::nonnegative_product(result, factor);
    }
    remaining >>= 1U;
    if (remaining != 0U) {
      factor = min_plus_detail::nonnegative_product(factor, factor);
    }
  }
  return min_plus_detail::narrow_nonnegative(result);
}

[[nodiscard]] inline MinPlusMatrix exact_walk_distances_nonnegative(
    const Graph& graph, std::uint64_t edge_count) {
  const std::size_t n = graph.vertex_count();
  MinPlusMatrix adjacency(n, n);

  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.weight < 0) {
        throw std::invalid_argument(
            "exact-edge min-plus walks require globally non-negative edges");
      }
      auto& current = adjacency.at(from, edge.to);
      if (!current.has_value() || edge.weight < *current) {
        current = edge.weight;
      }
    }
  }

  return min_plus_power_nonnegative(adjacency, edge_count);
}

}  // namespace algorithms::graphs
