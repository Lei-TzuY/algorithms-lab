#pragma once

#include "algorithms/topology/persistent_homology.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::topology {

struct ElementaryCollapsePair {
  FilteredSimplex free_face;
  FilteredSimplex coface;

  friend bool operator==(const ElementaryCollapsePair&, const ElementaryCollapsePair&) = default;
};

struct FilteredSimplicialCollapseResult {
  std::vector<FilteredSimplex> reduced_simplices;
  std::vector<ElementaryCollapsePair> collapses;
};

namespace collapse_detail {

inline std::vector<std::size_t> canonical_vertices(std::span<const std::size_t> vertices) {
  if (vertices.empty()) {
    throw std::invalid_argument("filtered collapse simplices must be non-empty");
  }
  std::vector<std::size_t> canonical(vertices.begin(), vertices.end());
  std::sort(canonical.begin(), canonical.end());
  if (std::adjacent_find(canonical.begin(), canonical.end()) != canonical.end()) {
    throw std::invalid_argument("filtered collapse simplex vertices must be distinct");
  }
  return canonical;
}

inline std::vector<std::size_t> face_without(const std::vector<std::size_t>& vertices,
                                              std::size_t removed) {
  std::vector<std::size_t> face;
  face.reserve(vertices.size() - 1);
  for (std::size_t position = 0; position < vertices.size(); ++position) {
    if (position != removed) {
      face.push_back(vertices[position]);
    }
  }
  return face;
}

inline bool output_order(const FilteredSimplex& left, const FilteredSimplex& right) {
  if (left.filtration != right.filtration) {
    return left.filtration < right.filtration;
  }
  if (left.vertices.size() != right.vertices.size()) {
    return left.vertices.size() < right.vertices.size();
  }
  return left.vertices < right.vertices;
}

}  // namespace collapse_detail

inline FilteredSimplicialCollapseResult filtration_preserving_elementary_collapse(
    std::span<const FilteredSimplex> simplices) {
  const std::size_t count = simplices.size();
  std::vector<FilteredSimplex> canonical;
  canonical.reserve(count);
  for (const auto& simplex : simplices) {
    canonical.push_back(
        FilteredSimplex{collapse_detail::canonical_vertices(simplex.vertices), simplex.filtration});
  }

  // Deterministic structural order: lower dimension first, then vertices.
  std::sort(canonical.begin(), canonical.end(),
            [](const FilteredSimplex& left, const FilteredSimplex& right) {
              if (left.vertices.size() != right.vertices.size()) {
                return left.vertices.size() < right.vertices.size();
              }
              return left.vertices < right.vertices;
            });

  std::map<std::vector<std::size_t>, std::size_t> index_of;
  for (std::size_t index = 0; index < count; ++index) {
    const auto [it, inserted] = index_of.emplace(canonical[index].vertices, index);
    static_cast<void>(it);
    if (!inserted) {
      throw std::invalid_argument("filtered collapse simplices must be unique");
    }
  }

  std::vector<std::vector<std::size_t>> immediate_cofaces(count);
  for (std::size_t coface_index = 0; coface_index < count; ++coface_index) {
    const auto& coface = canonical[coface_index];
    if (coface.vertices.size() == 1) {
      continue;
    }
    for (std::size_t removed = 0; removed < coface.vertices.size(); ++removed) {
      const auto face = collapse_detail::face_without(coface.vertices, removed);
      const auto found = index_of.find(face);
      if (found == index_of.end()) {
        throw std::invalid_argument("filtered collapse input must be downward-closed");
      }
      const std::size_t face_index = found->second;
      if (canonical[face_index].filtration > coface.filtration) {
        throw std::invalid_argument(
            "filtered collapse face filtration must not exceed coface filtration");
      }
      immediate_cofaces[face_index].push_back(coface_index);
    }
  }

  std::vector<unsigned char> active(count, 1);
  FilteredSimplicialCollapseResult result;
  result.collapses.reserve(count / 2);

  while (true) {
    bool collapsed = false;
    for (std::size_t face_index = 0; face_index < count; ++face_index) {
      if (active[face_index] == 0) {
        continue;
      }
      std::size_t unique_coface = count;
      std::size_t active_cofaces = 0;
      for (const std::size_t coface_index : immediate_cofaces[face_index]) {
        if (active[coface_index] == 0) {
          continue;
        }
        ++active_cofaces;
        unique_coface = coface_index;
        if (active_cofaces > 1) {
          break;
        }
      }
      if (active_cofaces != 1) {
        continue;
      }
      if (canonical[face_index].filtration != canonical[unique_coface].filtration) {
        continue;
      }

      result.collapses.push_back(
          ElementaryCollapsePair{canonical[face_index], canonical[unique_coface]});
      active[face_index] = 0;
      active[unique_coface] = 0;
      collapsed = true;
      break;
    }
    if (!collapsed) {
      break;
    }
  }

  result.reduced_simplices.reserve(count - 2 * result.collapses.size());
  for (std::size_t index = 0; index < count; ++index) {
    if (active[index] != 0) {
      result.reduced_simplices.push_back(canonical[index]);
    }
  }
  std::sort(result.reduced_simplices.begin(), result.reduced_simplices.end(),
            collapse_detail::output_order);
  return result;
}

}  // namespace algorithms::topology
