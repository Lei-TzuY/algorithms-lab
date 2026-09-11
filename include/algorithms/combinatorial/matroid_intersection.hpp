#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace algorithms::combinatorial {

using MatroidIndependenceOracle =
    std::function<bool(const std::vector<std::size_t>&)>;

struct MatroidIntersectionResult {
  std::vector<std::size_t> selected_elements;
  std::size_t augmentation_count{};
  std::size_t first_oracle_calls{};
  std::size_t second_oracle_calls{};

  friend bool operator==(const MatroidIntersectionResult&,
                         const MatroidIntersectionResult&) = default;
};

// Maximum-cardinality intersection of two matroids over ground elements
// [0, ground_size). The caller supplies exact independence predicates for both
// matroids. Every subset passed to an oracle is sorted increasingly.
//
// The implementation is the classical augmenting exchange-graph baseline:
// sources can be added directly in the first matroid, sinks can be added
// directly in the second, and alternating exchange arcs are searched by BFS.
// The result is deterministic for deterministic oracles.
//
// The empty set must be independent in both matroids; otherwise
// std::invalid_argument is thrown. General matroid axioms cannot be validated
// from a black-box predicate and remain a caller precondition.
[[nodiscard]] MatroidIntersectionResult maximum_cardinality_matroid_intersection(
    std::size_t ground_size, const MatroidIndependenceOracle& first,
    const MatroidIndependenceOracle& second);

}  // namespace algorithms::combinatorial
