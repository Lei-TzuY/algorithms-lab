#pragma once

#include "algorithms/combinatorial/matroid_intersection.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorial {

struct MatroidUnionResult {
  std::vector<std::size_t> selected_elements;
  std::vector<std::vector<std::size_t>> independent_sets;
  std::vector<std::optional<std::size_t>> assigned_matroid;
  std::size_t augmentation_count{};
  std::vector<std::size_t> oracle_calls;

  friend bool operator==(const MatroidUnionResult&,
                         const MatroidUnionResult&) = default;
};

// Maximum-cardinality union of caller-supplied matroids over the common ground
// set [0, ground_size).  A selected element is assigned to exactly one matroid,
// and each returned layer is independent in that matroid.
//
// The implementation reduces matroid union to the repository's first-principles
// matroid-intersection engine over copies (layer, element).  The first matroid is
// the direct sum of the supplied matroids; the second is a partition matroid that
// permits at most one copy of each original element.  Common independent copy
// sets are therefore exactly feasible matroid-union assignments.
//
// Every supplied oracle must be non-empty and accept the empty set.  General
// matroid axioms remain a caller precondition, as they cannot be certified from
// finitely many black-box independence queries.
[[nodiscard]] inline MatroidUnionResult maximum_cardinality_matroid_union(
    std::size_t ground_size,
    const std::vector<MatroidIndependenceOracle>& matroids) {
  MatroidUnionResult result;
  result.oracle_calls.assign(matroids.size(), 0U);

  const std::vector<std::size_t> empty;
  for (std::size_t layer = 0; layer < matroids.size(); ++layer) {
    if (!matroids[layer]) {
      throw std::invalid_argument("matroid independence oracle is empty");
    }
    ++result.oracle_calls[layer];
    if (!matroids[layer](empty)) {
      throw std::invalid_argument(
          "matroid independence oracle must accept the empty set");
    }
  }

  if (!matroids.empty() &&
      ground_size > std::numeric_limits<std::size_t>::max() / matroids.size()) {
    throw std::length_error("matroid-union copy ground size overflow");
  }
  const std::size_t copy_count = ground_size * matroids.size();

  result.independent_sets.resize(matroids.size());
  result.assigned_matroid.assign(ground_size, std::nullopt);
  if (copy_count == 0U) {
    return result;
  }

  auto direct_sum = [&](const std::vector<std::size_t>& copies) {
    std::vector<std::vector<std::size_t>> layer_elements(matroids.size());
    for (const std::size_t copy : copies) {
      if (copy >= copy_count) {
        throw std::logic_error("matroid-union copy outside expanded ground set");
      }
      const std::size_t layer = copy / ground_size;
      const std::size_t element = copy % ground_size;
      layer_elements[layer].push_back(element);
    }
    for (std::size_t layer = 0; layer < matroids.size(); ++layer) {
      if (layer_elements[layer].empty()) {
        continue;
      }
      if (result.oracle_calls[layer] ==
          std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("matroid-union oracle call counter overflow");
      }
      ++result.oracle_calls[layer];
      if (!matroids[layer](layer_elements[layer])) {
        return false;
      }
    }
    return true;
  };

  auto at_most_one_copy = [&](const std::vector<std::size_t>& copies) {
    std::vector<bool> seen(ground_size, false);
    for (const std::size_t copy : copies) {
      if (copy >= copy_count) {
        throw std::logic_error("matroid-union copy outside expanded ground set");
      }
      const std::size_t element = copy % ground_size;
      if (seen[element]) {
        return false;
      }
      seen[element] = true;
    }
    return true;
  };

  const MatroidIntersectionResult expanded =
      maximum_cardinality_matroid_intersection(copy_count, direct_sum,
                                               at_most_one_copy);
  result.augmentation_count = expanded.augmentation_count;

  for (const std::size_t copy : expanded.selected_elements) {
    const std::size_t layer = copy / ground_size;
    const std::size_t element = copy % ground_size;
    if (result.assigned_matroid[element].has_value()) {
      throw std::logic_error("matroid-union reduction selected duplicate element");
    }
    result.assigned_matroid[element] = layer;
    result.independent_sets[layer].push_back(element);
    result.selected_elements.push_back(element);
  }
  std::sort(result.selected_elements.begin(), result.selected_elements.end());
  for (auto& layer : result.independent_sets) {
    std::sort(layer.begin(), layer.end());
  }

  if (result.selected_elements.size() != result.augmentation_count) {
    throw std::logic_error("matroid-union augmentation count mismatch");
  }
  for (std::size_t layer = 0; layer < matroids.size(); ++layer) {
    if (result.oracle_calls[layer] == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("matroid-union oracle call counter overflow");
    }
    ++result.oracle_calls[layer];
    if (!matroids[layer](result.independent_sets[layer])) {
      throw std::logic_error("matroid-union layer is not independent");
    }
  }
  return result;
}

}  // namespace algorithms::combinatorial
