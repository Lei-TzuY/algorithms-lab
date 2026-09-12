#pragma once

#include "algorithms/data_structures/euler_tour_forest.hpp"

#include <cstddef>
#include <cstdint>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

class EulerTourForestOracle {
 public:
  explicit EulerTourForestOracle(std::size_t vertex_count)
      : adjacency_(vertex_count) {}

  [[nodiscard]] bool connected(std::size_t first, std::size_t second) const {
    std::vector<bool> seen(adjacency_.size(), false);
    std::queue<std::size_t> queue;
    seen[first] = true;
    queue.push(first);
    while (!queue.empty()) {
      const std::size_t vertex = queue.front();
      queue.pop();
      if (vertex == second) {
        return true;
      }
      for (const std::size_t neighbor : adjacency_[vertex]) {
        if (!seen[neighbor]) {
          seen[neighbor] = true;
          queue.push(neighbor);
        }
      }
    }
    return false;
  }

  [[nodiscard]] std::size_t component_size(std::size_t start) const {
    std::vector<bool> seen(adjacency_.size(), false);
    std::queue<std::size_t> queue;
    seen[start] = true;
    queue.push(start);
    std::size_t count = 0;
    while (!queue.empty()) {
      const std::size_t vertex = queue.front();
      queue.pop();
      ++count;
      for (const std::size_t neighbor : adjacency_[vertex]) {
        if (!seen[neighbor]) {
          seen[neighbor] = true;
          queue.push(neighbor);
        }
      }
    }
    return count;
  }

  void link(std::size_t first, std::size_t second) {
    adjacency_[first].insert(second);
    adjacency_[second].insert(first);
  }

  void cut(std::size_t first, std::size_t second) {
    adjacency_[first].erase(second);
    adjacency_[second].erase(first);
  }

  [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> edges() const {
    std::vector<std::pair<std::size_t, std::size_t>> result;
    for (std::size_t first = 0; first < adjacency_.size(); ++first) {
      for (const std::size_t second : adjacency_[first]) {
        if (first < second) {
          result.emplace_back(first, second);
        }
      }
    }
    return result;
  }

 private:
  std::vector<std::set<std::size_t>> adjacency_;
};

}  // namespace

TEST_CASE(euler_tour_forest_basic_link_cut_and_reverse_endpoint_regression) {
  using algorithms::data_structures::EulerTourForest;

  EulerTourForest forest(6, UINT64_C(0x4f3f2f1f0f00aa55));
  REQUIRE(forest.valid_structure());
  REQUIRE_EQ(forest.edge_count(), 0U);

  // Intentionally link with first > second. The implementation stores edge
  // tokens canonically; this regression caught an earlier orientation mismatch.
  forest.link(4, 1);
  forest.link(1, 3);
  forest.link(3, 2);
  REQUIRE(forest.connected(4, 2));
  REQUIRE_EQ(forest.component_size(1), 4U);
  REQUIRE_EQ(forest.edge_count(), 3U);
  REQUIRE(forest.valid_structure());

  forest.cut(1, 4);
  REQUIRE(!forest.connected(4, 2));
  REQUIRE_EQ(forest.component_size(4), 1U);
  REQUIRE_EQ(forest.component_size(2), 3U);
  REQUIRE(forest.valid_structure());

  forest.link(4, 5);
  forest.link(5, 2);
  REQUIRE_EQ(forest.component_size(4), 5U);
  REQUIRE(forest.valid_structure());

  forest.cut(3, 1);
  REQUIRE(!forest.connected(1, 2));
  REQUIRE(forest.connected(4, 2));
  REQUIRE(forest.valid_structure());
}

TEST_CASE(euler_tour_forest_validation_and_cycle_rejection) {
  using algorithms::data_structures::EulerTourForest;

  EulerTourForest empty(0);
  REQUIRE(empty.valid_structure());
  REQUIRE_THROWS_AS(empty.connected(0, 0), std::out_of_range);

  EulerTourForest forest(4);
  REQUIRE_THROWS_AS(forest.link(0, 0), std::invalid_argument);
  REQUIRE_THROWS_AS(forest.cut(0, 0), std::invalid_argument);
  REQUIRE_THROWS_AS(forest.link(0, 4), std::out_of_range);
  REQUIRE_THROWS_AS(forest.component_size(4), std::out_of_range);

  forest.link(0, 1);
  forest.link(1, 2);
  REQUIRE_THROWS_AS(forest.link(0, 2), std::invalid_argument);
  REQUIRE_THROWS_AS(forest.cut(0, 2), std::invalid_argument);
  REQUIRE(forest.valid_structure());

  forest.cut(1, 2);
  REQUIRE_THROWS_AS(forest.cut(1, 2), std::invalid_argument);
  REQUIRE(forest.valid_structure());
}

TEST_CASE(euler_tour_forest_randomized_differential_against_bfs_forest) {
  using algorithms::data_structures::EulerTourForest;

  constexpr std::size_t kVertices = 40;
  constexpr std::size_t kSteps = 20000;
  EulerTourForest forest(kVertices, UINT64_C(0x6a09e667f3bcc909));
  EulerTourForestOracle oracle(kVertices);
  std::mt19937_64 rng(UINT64_C(0xbb67ae8584caa73b));

  for (std::size_t step = 0; step < kSteps; ++step) {
    const auto current_edges = oracle.edges();
    const std::uint64_t choice = rng() % 100U;

    if (choice < 38U) {
      const std::size_t first = static_cast<std::size_t>(rng() % kVertices);
      const std::size_t second = static_cast<std::size_t>(rng() % kVertices);
      if (first != second && !oracle.connected(first, second)) {
        forest.link(first, second);
        oracle.link(first, second);
      }
    } else if (choice < 63U && !current_edges.empty()) {
      const std::size_t edge_index =
          static_cast<std::size_t>(rng() % current_edges.size());
      const auto [first, second] = current_edges[edge_index];
      if ((rng() & UINT64_C(1)) == 0U) {
        forest.cut(first, second);
      } else {
        forest.cut(second, first);
      }
      oracle.cut(first, second);
    } else {
      const std::size_t first = static_cast<std::size_t>(rng() % kVertices);
      const std::size_t second = static_cast<std::size_t>(rng() % kVertices);
      REQUIRE_EQ(forest.connected(first, second),
                 oracle.connected(first, second));
      REQUIRE_EQ(forest.component_size(first), oracle.component_size(first));
    }

    REQUIRE_EQ(forest.edge_count(), oracle.edges().size());
    REQUIRE(forest.valid_structure());

    if (step % 211U == 0U) {
      for (std::size_t first = 0; first < kVertices; ++first) {
        REQUIRE_EQ(forest.component_size(first), oracle.component_size(first));
        for (std::size_t second = 0; second < kVertices; ++second) {
          REQUIRE_EQ(forest.connected(first, second),
                     oracle.connected(first, second));
        }
      }
    }
  }
}
