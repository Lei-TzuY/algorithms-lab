#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::topology {

struct FilteredSimplex {
  std::vector<std::size_t> vertices;
  std::int64_t filtration{};

  friend bool operator==(const FilteredSimplex&, const FilteredSimplex&) = default;
};

struct PersistenceInterval {
  std::size_t dimension{};
  std::int64_t birth{};
  std::optional<std::int64_t> death;
  std::size_t birth_simplex{};
  std::optional<std::size_t> death_simplex;

  friend bool operator==(const PersistenceInterval&, const PersistenceInterval&) = default;
};

struct PersistentHomologyResult {
  std::vector<FilteredSimplex> ordered_simplices;
  std::vector<PersistenceInterval> intervals;
};

namespace detail {

inline std::vector<std::size_t> xor_sorted_columns(
    const std::vector<std::size_t>& left,
    const std::vector<std::size_t>& right) {
  std::vector<std::size_t> result;
  result.reserve(left.size() + right.size());
  std::size_t i = 0;
  std::size_t j = 0;
  while (i < left.size() || j < right.size()) {
    if (j == right.size() || (i < left.size() && left[i] < right[j])) {
      result.push_back(left[i]);
      ++i;
    } else if (i == left.size() || right[j] < left[i]) {
      result.push_back(right[j]);
      ++j;
    } else {
      ++i;
      ++j;
    }
  }
  return result;
}

inline std::vector<std::size_t> canonical_vertices(
    std::span<const std::size_t> vertices) {
  if (vertices.empty()) {
    throw std::invalid_argument("persistent homology simplices must be non-empty");
  }
  std::vector<std::size_t> canonical(vertices.begin(), vertices.end());
  std::sort(canonical.begin(), canonical.end());
  if (std::adjacent_find(canonical.begin(), canonical.end()) != canonical.end()) {
    throw std::invalid_argument("persistent homology simplex vertices must be distinct");
  }
  return canonical;
}

}  // namespace detail

inline PersistentHomologyResult persistent_homology_z2(
    std::span<const FilteredSimplex> simplices) {
  PersistentHomologyResult result;
  result.ordered_simplices.reserve(simplices.size());
  for (const auto& simplex : simplices) {
    result.ordered_simplices.push_back(
        FilteredSimplex{detail::canonical_vertices(simplex.vertices), simplex.filtration});
  }

  std::sort(result.ordered_simplices.begin(), result.ordered_simplices.end(),
            [](const FilteredSimplex& left, const FilteredSimplex& right) {
              if (left.filtration != right.filtration) {
                return left.filtration < right.filtration;
              }
              if (left.vertices.size() != right.vertices.size()) {
                return left.vertices.size() < right.vertices.size();
              }
              return left.vertices < right.vertices;
            });

  const std::size_t count = result.ordered_simplices.size();
  std::map<std::vector<std::size_t>, std::size_t> simplex_index;
  for (std::size_t index = 0; index < count; ++index) {
    const auto [iterator, inserted] =
        simplex_index.emplace(result.ordered_simplices[index].vertices, index);
    static_cast<void>(iterator);
    if (!inserted) {
      throw std::invalid_argument("persistent homology simplices must be unique");
    }
  }

  std::vector<std::vector<std::size_t>> boundaries(count);
  for (std::size_t index = 0; index < count; ++index) {
    const auto& simplex = result.ordered_simplices[index];
    if (simplex.vertices.size() == 1) {
      continue;
    }
    auto& boundary = boundaries[index];
    boundary.reserve(simplex.vertices.size());
    for (std::size_t removed = 0; removed < simplex.vertices.size(); ++removed) {
      std::vector<std::size_t> face;
      face.reserve(simplex.vertices.size() - 1);
      for (std::size_t position = 0; position < simplex.vertices.size(); ++position) {
        if (position != removed) {
          face.push_back(simplex.vertices[position]);
        }
      }
      const auto found = simplex_index.find(face);
      if (found == simplex_index.end()) {
        throw std::invalid_argument("persistent homology input must be downward-closed");
      }
      const std::size_t face_index = found->second;
      if (result.ordered_simplices[face_index].filtration > simplex.filtration) {
        throw std::invalid_argument(
            "persistent homology face filtration must not exceed coface filtration");
      }
      if (face_index >= index) {
        throw std::logic_error("persistent homology ordering failed to place face first");
      }
      boundary.push_back(face_index);
    }
    std::sort(boundary.begin(), boundary.end());
  }

  std::vector<std::vector<std::size_t>> reduced(count);
  std::map<std::size_t, std::size_t> pivot_owner;
  std::vector<unsigned char> positive(count, 0);
  std::vector<std::optional<std::size_t>> paired_death(count);

  for (std::size_t column_index = 0; column_index < count; ++column_index) {
    auto column = boundaries[column_index];
    while (!column.empty()) {
      const std::size_t pivot = column.back();
      const auto owner = pivot_owner.find(pivot);
      if (owner == pivot_owner.end()) {
        break;
      }
      column = detail::xor_sorted_columns(column, reduced[owner->second]);
    }
    if (column.empty()) {
      positive[column_index] = 1;
    } else {
      const std::size_t pivot = column.back();
      if (positive[pivot] == 0) {
        throw std::logic_error("persistent homology reduction paired a non-birth simplex");
      }
      pivot_owner.emplace(pivot, column_index);
      paired_death[pivot] = column_index;
    }
    reduced[column_index] = std::move(column);
  }

  result.intervals.reserve(count);
  for (std::size_t birth_index = 0; birth_index < count; ++birth_index) {
    if (positive[birth_index] == 0) {
      continue;
    }
    PersistenceInterval interval;
    interval.dimension = result.ordered_simplices[birth_index].vertices.size() - 1;
    interval.birth = result.ordered_simplices[birth_index].filtration;
    interval.birth_simplex = birth_index;
    if (paired_death[birth_index].has_value()) {
      const std::size_t death_index = *paired_death[birth_index];
      interval.death = result.ordered_simplices[death_index].filtration;
      interval.death_simplex = death_index;
    }
    result.intervals.push_back(interval);
  }

  std::sort(result.intervals.begin(), result.intervals.end(),
            [](const PersistenceInterval& left, const PersistenceInterval& right) {
              if (left.dimension != right.dimension) {
                return left.dimension < right.dimension;
              }
              if (left.birth != right.birth) {
                return left.birth < right.birth;
              }
              if (left.death.has_value() != right.death.has_value()) {
                return left.death.has_value();
              }
              if (left.death.has_value() && left.death != right.death) {
                return left.death < right.death;
              }
              return left.birth_simplex < right.birth_simplex;
            });
  return result;
}

}  // namespace algorithms::topology
