#pragma once

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/radix_dijkstra.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct ThorupZwickK2BunchEntry {
  Vertex vertex{};
  Weight distance{};

  friend bool operator==(const ThorupZwickK2BunchEntry&,
                         const ThorupZwickK2BunchEntry&) = default;
};

// k=2 Thorup-Zwick-style distance oracle for undirected, non-negative graphs.
//
// A_0 is the full vertex set. A_1 is sampled with exact probability
// 1/ceil(sqrt(n)) from mt19937_64 using rejection sampling before the
// integer residue test, avoiding modulo bias.
// Every connected component with no sampled vertex receives its smallest
// vertex as a forced landmark. A_2 is empty.
//
// For each vertex v:
//   pivot(v) is the nearest A_1 landmark (ties by smaller vertex id);
//   B(v) contains every reachable landmark and every non-landmark w with
//   d(v,w) < d(v,pivot(v)).
//
// Query(u,v) is symmetric. If either endpoint occurs in the other's bunch,
// the exact distance is returned. Otherwise the two pivot detours are
// considered and the smaller representable estimate is returned.
//
// For every finite exact distance d, the returned estimate D satisfies
// d <= D <= 3d. Different connected components return nullopt.
//
// Preprocessing intentionally uses the repository's exact radix-Dijkstra once
// per source and retains only pivots and bunch distances, not the APSP matrix.
class ThorupZwickK2Oracle {
 public:
  explicit ThorupZwickK2Oracle(
      const Graph& graph,
      const std::uint64_t seed = UINT64_C(0x54485A4B324F5243))
      : vertex_count_(graph.vertex_count()),
        seed_(seed),
        landmark_(vertex_count_, 0U),
        pivot_(vertex_count_),
        pivot_distance_(vertex_count_),
        bunch_(vertex_count_) {
    validate_graph(graph);
    if (vertex_count_ == 0U) {
      return;
    }

    const auto components = connected_components(graph);
    sample_landmarks(components);
    build_index(graph);
  }

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return vertex_count_;
  }

  [[nodiscard]] std::uint64_t seed() const noexcept {
    return seed_;
  }

  [[nodiscard]] std::size_t landmark_count() const noexcept {
    return landmarks_.size();
  }

  [[nodiscard]] const std::vector<Vertex>& landmarks() const noexcept {
    return landmarks_;
  }

  [[nodiscard]] bool is_landmark(const Vertex vertex) const {
    validate_vertex(vertex);
    return landmark_[vertex] != 0U;
  }

  [[nodiscard]] Vertex pivot(const Vertex vertex) const {
    validate_vertex(vertex);
    if (!pivot_[vertex].has_value()) {
      throw std::logic_error("Thorup-Zwick vertex has no component landmark");
    }
    return *pivot_[vertex];
  }

  [[nodiscard]] Weight pivot_distance(const Vertex vertex) const {
    validate_vertex(vertex);
    if (!pivot_distance_[vertex].has_value()) {
      throw std::logic_error(
          "Thorup-Zwick vertex has no pivot distance");
    }
    return *pivot_distance_[vertex];
  }

  [[nodiscard]] std::size_t bunch_size(const Vertex vertex) const {
    validate_vertex(vertex);
    return bunch_[vertex].size();
  }

  [[nodiscard]] std::optional<Weight> query(
      const Vertex first, const Vertex second) const {
    validate_vertex(first);
    validate_vertex(second);

    if (first == second) {
      return Weight{0};
    }

    std::optional<Weight> best;
    bool overflowed_candidate = false;

    const auto consider_exact =
        [&](const std::optional<Weight> distance) {
          if (!distance.has_value()) {
            return;
          }
          if (!best.has_value() || *distance < *best) {
            best = *distance;
          }
        };

    consider_exact(bunch_distance(second, first));
    consider_exact(bunch_distance(first, second));
    if (best.has_value()) {
      return best;
    }

    const auto consider_pivot_detour =
        [&](const Vertex pivot_owner, const Vertex other) {
          if (!pivot_[pivot_owner].has_value() ||
              !pivot_distance_[pivot_owner].has_value()) {
            return;
          }

          const Vertex landmark = *pivot_[pivot_owner];
          const auto other_to_landmark =
              bunch_distance(other, landmark);
          if (!other_to_landmark.has_value()) {
            return;
          }

          const Weight owner_to_landmark =
              *pivot_distance_[pivot_owner];
          if (owner_to_landmark >
              std::numeric_limits<Weight>::max() -
                  *other_to_landmark) {
            overflowed_candidate = true;
            return;
          }

          const Weight candidate =
              owner_to_landmark + *other_to_landmark;
          if (!best.has_value() || candidate < *best) {
            best = candidate;
          }
        };

    consider_pivot_detour(second, first);
    consider_pivot_detour(first, second);

    if (!best.has_value() && overflowed_candidate) {
      throw std::overflow_error(
          "Thorup-Zwick query estimate exceeds Weight range");
    }
    return best;
  }

 private:
  std::size_t vertex_count_{};
  std::uint64_t seed_{};
  std::vector<unsigned char> landmark_;
  std::vector<Vertex> landmarks_;
  std::vector<std::optional<Vertex>> pivot_;
  std::vector<std::optional<Weight>> pivot_distance_;
  std::vector<std::vector<ThorupZwickK2BunchEntry>> bunch_;

  static void validate_graph(const Graph& graph) {
    if (graph.directed()) {
      throw std::invalid_argument(
          "Thorup-Zwick k=2 oracle requires an undirected graph");
    }

    for (const auto& adjacency : graph.adjacency()) {
      for (const Edge& edge : adjacency) {
        if (edge.weight < 0) {
          throw std::invalid_argument(
              "Thorup-Zwick k=2 oracle requires non-negative weights");
        }
      }
    }
  }

  [[nodiscard]] static std::size_t ceil_sqrt(
      const std::size_t value) noexcept {
    if (value <= 1U) {
      return value;
    }

    std::size_t low = 1U;
    std::size_t high = value;
    while (low < high) {
      const std::size_t middle =
          low + (high - low) / 2U;
      const std::size_t quotient = value / middle;
      const std::size_t remainder = value % middle;
      const bool square_reaches_value =
          middle > quotient ||
          (middle == quotient && remainder == 0U);

      if (square_reaches_value) {
        high = middle;
      } else {
        low = middle + 1U;
      }
    }
    return low;
  }

  [[nodiscard]] static std::vector<std::vector<Vertex>>
  connected_components(const Graph& graph) {
    const std::size_t n = graph.vertex_count();
    std::vector<unsigned char> seen(n, 0U);
    std::vector<std::vector<Vertex>> components;

    for (Vertex start = 0U; start < n; ++start) {
      if (seen[start] != 0U) {
        continue;
      }

      components.emplace_back();
      auto& component = components.back();
      std::vector<Vertex> stack{start};
      seen[start] = 1U;

      while (!stack.empty()) {
        const Vertex vertex = stack.back();
        stack.pop_back();
        component.push_back(vertex);

        for (const Edge& edge : graph.neighbors(vertex)) {
          if (seen[edge.to] == 0U) {
            seen[edge.to] = 1U;
            stack.push_back(edge.to);
          }
        }
      }

      std::sort(component.begin(), component.end());
    }

    return components;
  }

  void sample_landmarks(
      const std::vector<std::vector<Vertex>>& components) {
    const std::size_t denominator = ceil_sqrt(vertex_count_);
    if (denominator == 0U) {
      return;
    }

    std::mt19937_64 random(seed_);
    const auto divisor =
        static_cast<std::uint64_t>(denominator);
    const std::uint64_t rejection_threshold =
        (std::uint64_t{0} - divisor) % divisor;

    for (Vertex vertex = 0U; vertex < vertex_count_; ++vertex) {
      std::uint64_t draw = 0U;
      do {
        draw = random();
      } while (draw < rejection_threshold);

      if (draw % divisor == 0U) {
        landmark_[vertex] = 1U;
      }
    }

    for (const auto& component : components) {
      bool has_landmark = false;
      for (const Vertex vertex : component) {
        if (landmark_[vertex] != 0U) {
          has_landmark = true;
          break;
        }
      }
      if (!has_landmark && !component.empty()) {
        landmark_[component.front()] = 1U;
      }
    }

    for (Vertex vertex = 0U; vertex < vertex_count_; ++vertex) {
      if (landmark_[vertex] != 0U) {
        landmarks_.push_back(vertex);
      }
    }
  }

  void build_index(const Graph& graph) {
    for (Vertex source = 0U; source < vertex_count_; ++source) {
      const RadixShortestPathResult shortest =
          radix_heap_dijkstra(graph, source);

      std::optional<Vertex> best_landmark;
      std::optional<Weight> best_distance;
      for (const Vertex landmark : landmarks_) {
        const auto distance = shortest.distance[landmark];
        if (!distance.has_value()) {
          continue;
        }

        if (!best_distance.has_value() ||
            *distance < *best_distance ||
            (*distance == *best_distance &&
             landmark < *best_landmark)) {
          best_landmark = landmark;
          best_distance = *distance;
        }
      }

      if (!best_landmark.has_value() ||
          !best_distance.has_value()) {
        throw std::logic_error(
            "Thorup-Zwick component has no reachable landmark");
      }

      pivot_[source] = *best_landmark;
      pivot_distance_[source] = *best_distance;

      auto& source_bunch = bunch_[source];
      for (Vertex candidate = 0U;
           candidate < vertex_count_; ++candidate) {
        const auto distance = shortest.distance[candidate];
        if (!distance.has_value()) {
          continue;
        }

        if (landmark_[candidate] != 0U ||
            *distance < *best_distance) {
          source_bunch.push_back(
              ThorupZwickK2BunchEntry{candidate, *distance});
        }
      }
    }
  }

  [[nodiscard]] std::optional<Weight> bunch_distance(
      const Vertex owner, const Vertex target) const {
    const auto& entries = bunch_[owner];
    const auto iterator = std::lower_bound(
        entries.begin(), entries.end(), target,
        [](const ThorupZwickK2BunchEntry& entry,
           const Vertex vertex) {
          return entry.vertex < vertex;
        });
    if (iterator == entries.end() ||
        iterator->vertex != target) {
      return std::nullopt;
    }
    return iterator->distance;
  }

  void validate_vertex(const Vertex vertex) const {
    if (vertex >= vertex_count_) {
      throw std::out_of_range(
          "Thorup-Zwick vertex index out of range");
    }
  }
};

}  // namespace algorithms::graphs
