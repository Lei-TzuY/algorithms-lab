#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct BipartiteDegreeRealization {
  std::size_t left_count{};
  std::size_t right_count{};
  std::vector<std::pair<std::size_t, std::size_t>> edges;
  std::size_t reduction_steps{};

  friend bool operator==(const BipartiteDegreeRealization&,
                         const BipartiteDegreeRealization&) = default;
};

namespace bipartite_degree_sequence_detail {

inline std::uint64_t checked_add_u64(std::uint64_t lhs, std::uint64_t rhs) {
  if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs) {
    throw std::overflow_error("bipartite degree sum is not representable");
  }
  return lhs + rhs;
}

inline std::uint64_t checked_size_to_u64(std::size_t value) {
  if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
    if (value > static_cast<std::size_t>(
                    std::numeric_limits<std::uint64_t>::max())) {
      throw std::overflow_error("bipartite degree is not representable");
    }
  }
  return static_cast<std::uint64_t>(value);
}

inline void validate_degree_domain(std::span<const std::size_t> left_degrees,
                                   std::span<const std::size_t> right_degrees) {
  const std::size_t left_count = left_degrees.size();
  const std::size_t right_count = right_degrees.size();
  for (const std::size_t degree : left_degrees) {
    if (degree > right_count) {
      throw std::invalid_argument(
          "left bipartite degree exceeds opposite partition size");
    }
  }
  for (const std::size_t degree : right_degrees) {
    if (degree > left_count) {
      throw std::invalid_argument(
          "right bipartite degree exceeds opposite partition size");
    }
  }
}

inline std::uint64_t degree_sum(std::span<const std::size_t> degrees) {
  std::uint64_t total = 0;
  for (const std::size_t degree : degrees) {
    total = checked_add_u64(total, checked_size_to_u64(degree));
  }
  return total;
}

}  // namespace bipartite_degree_sequence_detail

// Gale-Ryser graphicality criterion for a simple bipartite graph with labelled
// left/right vertices. Vertex labels do not affect feasibility; each side is
// sorted only for the majorization check.
//
// For nonincreasing left degrees r and right degrees c with equal total sum,
// graphicality is equivalent to
//   sum_{i < k} r[i] <= sum_j min(k, c[j])
// for every 1 <= k <= |left|.
//
// Direct educational baseline: O(L log L + R log R + L*R) time and O(L+R)
// working storage.
inline bool gale_ryser_bipartite_graphical(
    std::span<const std::size_t> left_degrees,
    std::span<const std::size_t> right_degrees) {
  bipartite_degree_sequence_detail::validate_degree_domain(left_degrees,
                                                            right_degrees);
  if (bipartite_degree_sequence_detail::degree_sum(left_degrees) !=
      bipartite_degree_sequence_detail::degree_sum(right_degrees)) {
    return false;
  }

  std::vector<std::size_t> left(left_degrees.begin(), left_degrees.end());
  std::vector<std::size_t> right(right_degrees.begin(), right_degrees.end());
  std::sort(left.begin(), left.end(), std::greater<>());
  std::sort(right.begin(), right.end(), std::greater<>());

  std::uint64_t prefix = 0;
  for (std::size_t k = 1U; k <= left.size(); ++k) {
    prefix = bipartite_degree_sequence_detail::checked_add_u64(
        prefix,
        bipartite_degree_sequence_detail::checked_size_to_u64(left[k - 1U]));

    std::uint64_t bound = 0;
    for (const std::size_t degree : right) {
      bound = bipartite_degree_sequence_detail::checked_add_u64(
          bound, bipartite_degree_sequence_detail::checked_size_to_u64(
                     std::min(k, degree)));
    }
    if (prefix > bound) {
      return false;
    }
  }
  return true;
}

// Deterministic bipartite Havel-Hakimi realization. Left vertices are processed
// by descending requested degree, then ascending original id. At each reduction
// the chosen left vertex is connected to the right vertices with largest
// residual degree, ties by ascending right id.
//
// The bipartite Havel-Hakimi reduction theorem is the correctness obligation:
// replacing one left degree d by edges to the d largest right residual degrees
// preserves realizability. The direct implementation repeatedly sorts the right
// residuals, giving O(L*R log R + L log L + E) time and O(L+R+E) storage.
inline std::optional<BipartiteDegreeRealization>
 bipartite_havel_hakimi_realization(
    std::span<const std::size_t> left_degrees,
    std::span<const std::size_t> right_degrees) {
  bipartite_degree_sequence_detail::validate_degree_domain(left_degrees,
                                                            right_degrees);

  struct ResidualVertex {
    std::size_t degree{};
    std::size_t vertex{};
  };

  std::vector<ResidualVertex> left;
  left.reserve(left_degrees.size());
  for (std::size_t vertex = 0; vertex < left_degrees.size(); ++vertex) {
    left.push_back(ResidualVertex{left_degrees[vertex], vertex});
  }
  std::sort(left.begin(), left.end(), [](const ResidualVertex& lhs,
                                         const ResidualVertex& rhs) {
    if (lhs.degree != rhs.degree) {
      return lhs.degree > rhs.degree;
    }
    return lhs.vertex < rhs.vertex;
  });

  std::vector<ResidualVertex> right;
  right.reserve(right_degrees.size());
  for (std::size_t vertex = 0; vertex < right_degrees.size(); ++vertex) {
    right.push_back(ResidualVertex{right_degrees[vertex], vertex});
  }

  BipartiteDegreeRealization result;
  result.left_count = left_degrees.size();
  result.right_count = right_degrees.size();

  const auto residual_order = [](const ResidualVertex& lhs,
                                 const ResidualVertex& rhs) {
    if (lhs.degree != rhs.degree) {
      return lhs.degree > rhs.degree;
    }
    return lhs.vertex < rhs.vertex;
  };

  for (const ResidualVertex chosen : left) {
    if (chosen.degree == 0U) {
      continue;
    }
    ++result.reduction_steps;
    std::sort(right.begin(), right.end(), residual_order);
    if (chosen.degree > right.size()) {
      return std::nullopt;
    }
    for (std::size_t index = 0; index < chosen.degree; ++index) {
      if (right[index].degree == 0U) {
        return std::nullopt;
      }
      result.edges.emplace_back(chosen.vertex, right[index].vertex);
      --right[index].degree;
    }
  }

  if (std::any_of(right.begin(), right.end(),
                  [](const ResidualVertex& vertex) {
                    return vertex.degree != 0U;
                  })) {
    return std::nullopt;
  }

  std::sort(result.edges.begin(), result.edges.end());
  if (std::adjacent_find(result.edges.begin(), result.edges.end()) !=
      result.edges.end()) {
    throw std::logic_error(
        "bipartite Havel-Hakimi produced a duplicate edge");
  }
  return result;
}

inline bool valid_bipartite_degree_realization(
    std::span<const std::size_t> left_degrees,
    std::span<const std::size_t> right_degrees,
    const BipartiteDegreeRealization& realization) {
  if (realization.left_count != left_degrees.size() ||
      realization.right_count != right_degrees.size()) {
    return false;
  }

  std::vector<std::size_t> left_replay(left_degrees.size(), 0U);
  std::vector<std::size_t> right_replay(right_degrees.size(), 0U);
  std::pair<std::size_t, std::size_t> previous{};
  bool have_previous = false;
  for (const auto& edge : realization.edges) {
    const auto [left, right] = edge;
    if (left >= left_degrees.size() || right >= right_degrees.size()) {
      return false;
    }
    if (have_previous && !(previous < edge)) {
      return false;
    }
    previous = edge;
    have_previous = true;
    ++left_replay[left];
    ++right_replay[right];
  }

  return std::equal(left_replay.begin(), left_replay.end(),
                    left_degrees.begin(), left_degrees.end()) &&
         std::equal(right_replay.begin(), right_replay.end(),
                    right_degrees.begin(), right_degrees.end());
}

}  // namespace algorithms::graphs
