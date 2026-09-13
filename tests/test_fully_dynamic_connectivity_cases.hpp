#pragma once

#include "algorithms/data_structures/fully_dynamic_connectivity.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fully_dynamic_connectivity_tests {

using algorithms::data_structures::FullyDynamicConnectivity;

struct EdgeKey {
  std::size_t first = 0;
  std::size_t second = 0;

  friend bool operator<(const EdgeKey& lhs, const EdgeKey& rhs) noexcept {
    return lhs.first < rhs.first ||
           (lhs.first == rhs.first && lhs.second < rhs.second);
  }
};

[[nodiscard]] inline EdgeKey canonical_key(std::size_t first,
                                            std::size_t second) {
  return first <= second ? EdgeKey{first, second}
                         : EdgeKey{second, first};
}

class NaiveDynamicConnectivity {
 public:
  explicit NaiveDynamicConnectivity(std::size_t vertex_count)
      : vertex_count_(vertex_count) {}

  void add_edge(std::size_t first, std::size_t second) {
    ++edges_[canonical_key(first, second)];
    ++edge_count_;
  }

  void remove_edge(std::size_t first, std::size_t second) {
    auto it = edges_.find(canonical_key(first, second));
    if (it == edges_.end()) {
      throw std::invalid_argument("naive remove requires active edge");
    }
    --it->second;
    --edge_count_;
    if (it->second == 0) {
      edges_.erase(it);
    }
  }

  [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }

  [[nodiscard]] std::size_t multiplicity(std::size_t first,
                                         std::size_t second) const {
    const auto it = edges_.find(canonical_key(first, second));
    return it == edges_.end() ? 0U : it->second;
  }

  [[nodiscard]] bool connected(std::size_t first, std::size_t second) const {
    if (first == second) {
      return true;
    }
    const auto adjacency = build_adjacency();
    std::vector<bool> seen(vertex_count_, false);
    std::queue<std::size_t> queue;
    seen[first] = true;
    queue.push(first);
    while (!queue.empty()) {
      const std::size_t vertex = queue.front();
      queue.pop();
      for (const std::size_t next : adjacency[vertex]) {
        if (seen[next]) {
          continue;
        }
        if (next == second) {
          return true;
        }
        seen[next] = true;
        queue.push(next);
      }
    }
    return false;
  }

  [[nodiscard]] std::size_t component_size(std::size_t start) const {
    const auto adjacency = build_adjacency();
    std::vector<bool> seen(vertex_count_, false);
    std::queue<std::size_t> queue;
    seen[start] = true;
    queue.push(start);
    std::size_t count = 0;
    while (!queue.empty()) {
      const std::size_t vertex = queue.front();
      queue.pop();
      ++count;
      for (const std::size_t next : adjacency[vertex]) {
        if (!seen[next]) {
          seen[next] = true;
          queue.push(next);
        }
      }
    }
    return count;
  }

  [[nodiscard]] std::vector<EdgeKey> active_keys() const {
    std::vector<EdgeKey> result;
    result.reserve(edges_.size());
    for (const auto& [key, multiplicity] : edges_) {
      static_cast<void>(multiplicity);
      result.push_back(key);
    }
    return result;
  }

 private:
  [[nodiscard]] std::vector<std::vector<std::size_t>> build_adjacency() const {
    std::vector<std::vector<std::size_t>> adjacency(vertex_count_);
    for (const auto& [key, multiplicity] : edges_) {
      if (multiplicity == 0 || key.first == key.second) {
        continue;
      }
      adjacency[key.first].push_back(key.second);
      adjacency[key.second].push_back(key.first);
    }
    return adjacency;
  }

  std::size_t vertex_count_ = 0;
  std::map<EdgeKey, std::size_t> edges_;
  std::size_t edge_count_ = 0;
};

inline void require_same_state(const FullyDynamicConnectivity& actual,
                               const NaiveDynamicConnectivity& expected,
                               std::size_t vertex_count) {
  REQUIRE_EQ(actual.edge_count(), expected.edge_count());
  REQUIRE(actual.tree_edge_count() <=
          (vertex_count == 0 ? 0U : vertex_count - 1U));
  REQUIRE_EQ(actual.non_tree_edge_count(),
             actual.edge_count() - actual.tree_edge_count());
  REQUIRE(actual.valid_structure());

  for (std::size_t first = 0; first < vertex_count; ++first) {
    REQUIRE_EQ(actual.component_size(first), expected.component_size(first));
    for (std::size_t second = 0; second < vertex_count; ++second) {
      REQUIRE_EQ(actual.connected(first, second),
                 expected.connected(first, second));
      REQUIRE_EQ(actual.multiplicity(first, second),
                 expected.multiplicity(first, second));
    }
  }
}

TEST_CASE(fully_dynamic_connectivity_validates_vertices_and_inactive_removal) {
  FullyDynamicConnectivity empty(0);
  REQUIRE_EQ(empty.edge_count(), 0U);
  REQUIRE(empty.valid_structure());
  REQUIRE_THROWS_AS(empty.connected(0, 0), std::out_of_range);
  REQUIRE_THROWS_AS(empty.add_edge(0, 0), std::out_of_range);

  FullyDynamicConnectivity graph(3);
  REQUIRE_THROWS_AS(graph.add_edge(3, 0), std::out_of_range);
  REQUIRE_THROWS_AS(graph.remove_edge(0, 1), std::invalid_argument);
  REQUIRE_THROWS_AS(graph.component_size(3), std::out_of_range);
  REQUIRE(graph.valid_structure());
}

TEST_CASE(fully_dynamic_connectivity_parallel_self_loop_and_replacement_semantics) {
  FullyDynamicConnectivity graph(4, 7);

  graph.add_edge(0, 0);
  graph.add_edge(0, 0);
  REQUIRE_EQ(graph.multiplicity(0, 0), 2U);
  REQUIRE_EQ(graph.tree_edge_count(), 0U);
  REQUIRE_EQ(graph.non_tree_edge_count(), 2U);

  graph.add_edge(0, 1);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  graph.add_edge(0, 2);
  REQUIRE_EQ(graph.multiplicity(1, 0), 2U);
  REQUIRE_EQ(graph.tree_edge_count(), 2U);
  REQUIRE(graph.connected(0, 2));
  REQUIRE(graph.valid_structure());

  // Parallel non-tree copies are removed before the represented tree copy.
  graph.remove_edge(1, 0);
  REQUIRE_EQ(graph.multiplicity(0, 1), 1U);
  REQUIRE(graph.connected(0, 1));

  // Removing a represented tree edge promotes the crossing 0-2 edge.
  graph.remove_edge(1, 2);
  REQUIRE(graph.connected(0, 2));
  REQUIRE_EQ(graph.tree_edge_count(), 2U);
  REQUIRE(graph.valid_structure());

  // Removing that replacement now really separates vertex 2.
  graph.remove_edge(2, 0);
  REQUIRE(!graph.connected(0, 2));
  REQUIRE_EQ(graph.component_size(2), 1U);

  graph.remove_edge(0, 0);
  graph.remove_edge(0, 0);
  REQUIRE_EQ(graph.multiplicity(0, 0), 0U);
  REQUIRE(graph.valid_structure());
}

TEST_CASE(fully_dynamic_connectivity_endpoint_symmetry_and_component_sizes) {
  FullyDynamicConnectivity graph(7, 1234);
  graph.add_edge(4, 1);
  graph.add_edge(1, 3);
  graph.add_edge(3, 5);
  graph.add_edge(5, 4);
  graph.add_edge(2, 6);

  REQUIRE(graph.connected(1, 5));
  REQUIRE(graph.connected(5, 1));
  REQUIRE_EQ(graph.component_size(4), 4U);
  REQUIRE_EQ(graph.component_size(2), 2U);
  REQUIRE_EQ(graph.component_size(0), 1U);
  REQUIRE_EQ(graph.multiplicity(1, 4), 1U);
  REQUIRE_EQ(graph.multiplicity(4, 1), 1U);

  graph.remove_edge(3, 1);
  REQUIRE(graph.connected(1, 3));
  graph.remove_edge(4, 5);
  REQUIRE(!graph.connected(1, 3));
  REQUIRE(graph.valid_structure());
}

TEST_CASE(fully_dynamic_connectivity_randomized_differential_against_rebuilt_bfs) {
  constexpr std::size_t vertex_count = 18;
  constexpr int operation_count = 6000;
  FullyDynamicConnectivity actual(vertex_count, 0xD1A6C0ULL);
  NaiveDynamicConnectivity expected(vertex_count);
  std::mt19937_64 rng(0xF011D1A6C0ULL);

  for (int step = 0; step < operation_count; ++step) {
    const std::size_t first =
        static_cast<std::size_t>(rng() % vertex_count);
    const std::size_t second =
        static_cast<std::size_t>(rng() % vertex_count);
    const std::uint64_t choice = rng() % 100U;

    if (choice < 47U) {
      actual.add_edge(first, second);
      expected.add_edge(first, second);
    } else if (choice < 72U && expected.edge_count() != 0U) {
      const auto keys = expected.active_keys();
      const EdgeKey selected =
          keys[static_cast<std::size_t>(rng() % keys.size())];
      actual.remove_edge(selected.first, selected.second);
      expected.remove_edge(selected.first, selected.second);
    } else {
      REQUIRE_EQ(actual.connected(first, second),
                 expected.connected(first, second));
      REQUIRE_EQ(actual.component_size(first),
                 expected.component_size(first));
    }

    REQUIRE_EQ(actual.edge_count(), expected.edge_count());
    REQUIRE(actual.valid_structure());

    if ((step % 120) == 0) {
      require_same_state(actual, expected, vertex_count);
    }
  }

  require_same_state(actual, expected, vertex_count);
}

}  // namespace fully_dynamic_connectivity_tests
