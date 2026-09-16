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

struct DegreeSequenceRealization {
  std::size_t vertex_count{};
  std::vector<std::pair<std::size_t, std::size_t>> edges;
  std::size_t reduction_steps{};

  friend bool operator==(const DegreeSequenceRealization&,
                         const DegreeSequenceRealization&) = default;
};

namespace detail {

inline std::uint64_t checked_add_u64(std::uint64_t lhs, std::uint64_t rhs) {
  if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs) {
    throw std::overflow_error("degree-sequence sum is not representable");
  }
  return lhs + rhs;
}

inline std::uint64_t checked_size_to_u64(std::size_t value) {
  if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
    if (value > static_cast<std::size_t>(
                    std::numeric_limits<std::uint64_t>::max())) {
      throw std::overflow_error("degree value is not representable");
    }
  }
  return static_cast<std::uint64_t>(value);
}

inline void validate_degree_domain(std::span<const std::size_t> degrees) {
  const std::size_t n = degrees.size();
  for (const std::size_t degree : degrees) {
    if (degree >= n && n != 0U) {
      throw std::invalid_argument("simple-graph degree must be smaller than vertex count");
    }
    if (n == 0U && degree != 0U) {
      throw std::invalid_argument("empty degree sequence cannot contain entries");
    }
  }
}

}  // namespace detail

// Erdős-Gallai graphicality criterion for a simple undirected graph.
// This direct educational baseline uses an O(n^2) right-hand-side scan after
// sorting, rather than the prefix/binary-search optimization.
inline bool erdos_gallai_graphical(std::span<const std::size_t> degrees) {
  detail::validate_degree_domain(degrees);
  const std::size_t n = degrees.size();
  if (n == 0U) {
    return true;
  }

  std::vector<std::size_t> sorted(degrees.begin(), degrees.end());
  std::sort(sorted.begin(), sorted.end(), std::greater<>());

  std::uint64_t total = 0;
  for (const std::size_t degree : sorted) {
    total = detail::checked_add_u64(total, detail::checked_size_to_u64(degree));
  }
  if ((total & 1U) != 0U) {
    return false;
  }

  std::uint64_t left = 0;
  for (std::size_t k = 1; k <= n; ++k) {
    left = detail::checked_add_u64(left,
                                   detail::checked_size_to_u64(sorted[k - 1U]));

    std::uint64_t right = 0;
    const std::uint64_t k64 = detail::checked_size_to_u64(k);
    for (std::size_t i = 0; i < k; ++i) {
      right = detail::checked_add_u64(right, k64 - 1U);
    }
    for (std::size_t i = k; i < n; ++i) {
      const std::size_t capped = std::min(sorted[i], k);
      right = detail::checked_add_u64(right,
                                      detail::checked_size_to_u64(capped));
    }
    if (left > right) {
      return false;
    }
  }
  return true;
}

// Deterministic Havel-Hakimi construction. At every reduction, the vertex with
// largest residual degree is selected; ties use the smaller original vertex id.
// It is connected to the next d vertices in the same ordering. The returned
// witness is sorted by endpoint pair.
inline std::optional<DegreeSequenceRealization> havel_hakimi_realization(
    std::span<const std::size_t> degrees) {
  detail::validate_degree_domain(degrees);
  const std::size_t n = degrees.size();
  struct ResidualVertex {
    std::size_t degree{};
    std::size_t vertex{};
  };

  std::vector<ResidualVertex> residual;
  residual.reserve(n);
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    residual.push_back(ResidualVertex{degrees[vertex], vertex});
  }

  DegreeSequenceRealization result;
  result.vertex_count = n;

  const auto residual_order = [](const ResidualVertex& lhs,
                                 const ResidualVertex& rhs) {
    if (lhs.degree != rhs.degree) {
      return lhs.degree > rhs.degree;
    }
    return lhs.vertex < rhs.vertex;
  };

  while (true) {
    std::sort(residual.begin(), residual.end(), residual_order);
    if (residual.empty() || residual.front().degree == 0U) {
      break;
    }

    const ResidualVertex chosen = residual.front();
    residual.erase(residual.begin());
    ++result.reduction_steps;

    if (chosen.degree > residual.size()) {
      return std::nullopt;
    }
    for (std::size_t index = 0; index < chosen.degree; ++index) {
      if (residual[index].degree == 0U) {
        return std::nullopt;
      }
      const std::size_t other = residual[index].vertex;
      result.edges.emplace_back(std::min(chosen.vertex, other),
                                std::max(chosen.vertex, other));
      --residual[index].degree;
    }
  }

  std::sort(result.edges.begin(), result.edges.end());
  if (std::adjacent_find(result.edges.begin(), result.edges.end()) !=
      result.edges.end()) {
    throw std::logic_error("Havel-Hakimi produced a duplicate edge");
  }
  return result;
}

inline bool valid_degree_sequence_realization(
    std::span<const std::size_t> degrees,
    const DegreeSequenceRealization& realization) {
  if (realization.vertex_count != degrees.size()) {
    return false;
  }
  std::vector<std::size_t> replay(degrees.size(), 0U);
  std::pair<std::size_t, std::size_t> previous{};
  bool have_previous = false;
  for (const auto& edge : realization.edges) {
    const auto [u, v] = edge;
    if (u >= degrees.size() || v >= degrees.size() || u >= v) {
      return false;
    }
    if (have_previous && !(previous < edge)) {
      return false;
    }
    previous = edge;
    have_previous = true;
    ++replay[u];
    ++replay[v];
  }
  return std::equal(replay.begin(), replay.end(), degrees.begin(), degrees.end());
}

}  // namespace algorithms::graphs
