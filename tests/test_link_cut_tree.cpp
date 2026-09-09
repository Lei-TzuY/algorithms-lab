#include "algorithms/graphs/link_cut_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::graphs::LinkCutForest;
using algorithms::graphs::Vertex;

namespace {

class NaiveForest {
 public:
  explicit NaiveForest(std::size_t vertex_count)
      : adjacency_(vertex_count, std::vector<bool>(vertex_count, false)) {}

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return adjacency_.size();
  }

  [[nodiscard]] bool has_edge(Vertex first, Vertex second) const {
    return adjacency_[first][second];
  }

  [[nodiscard]] bool connected(Vertex first, Vertex second) const {
    return distance(first, second).has_value();
  }

  [[nodiscard]] std::optional<std::size_t> distance(Vertex first,
                                                    Vertex second) const {
    std::vector<bool> visited(vertex_count(), false);
    std::queue<std::pair<Vertex, std::size_t>> queue;
    visited[first] = true;
    queue.emplace(first, 0);
    while (!queue.empty()) {
      const auto [vertex, depth] = queue.front();
      queue.pop();
      if (vertex == second) {
        return depth;
      }
      for (Vertex neighbor = 0; neighbor < vertex_count(); ++neighbor) {
        if (adjacency_[vertex][neighbor] && !visited[neighbor]) {
          visited[neighbor] = true;
          queue.emplace(neighbor, depth + 1);
        }
      }
    }
    return std::nullopt;
  }

  void link(Vertex first, Vertex second) {
    adjacency_[first][second] = true;
    adjacency_[second][first] = true;
  }

  void cut(Vertex first, Vertex second) {
    adjacency_[first][second] = false;
    adjacency_[second][first] = false;
  }

  [[nodiscard]] std::vector<std::pair<Vertex, Vertex>> edges() const {
    std::vector<std::pair<Vertex, Vertex>> result;
    for (Vertex first = 0; first < vertex_count(); ++first) {
      for (Vertex second = first + 1; second < vertex_count(); ++second) {
        if (adjacency_[first][second]) {
          result.emplace_back(first, second);
        }
      }
    }
    return result;
  }

 private:
  std::vector<std::vector<bool>> adjacency_;
};

Vertex random_vertex(std::mt19937_64& generator, std::size_t vertex_count) {
  return static_cast<Vertex>(generator() %
                             static_cast<std::uint64_t>(vertex_count));
}

}  // namespace

TEST_CASE(link_cut_tree_basic_link_cut_distance_and_rejections) {
  LinkCutForest forest(7);
  REQUIRE_EQ(forest.vertex_count(), static_cast<std::size_t>(7));
  REQUIRE(forest.valid_auxiliary_invariants());

  forest.link(0, 1);
  forest.link(1, 2);
  forest.link(2, 3);
  forest.link(3, 4);
  REQUIRE(forest.connected(0, 4));
  REQUIRE_EQ(forest.path_edge_distance(0, 4), static_cast<std::size_t>(4));
  REQUIRE_EQ(forest.path_edge_distance(4, 0), static_cast<std::size_t>(4));
  REQUIRE_EQ(forest.path_edge_distance(2, 2), static_cast<std::size_t>(0));
  REQUIRE(!forest.connected(0, 6));
  REQUIRE_THROWS_AS(forest.path_edge_distance(0, 6), std::invalid_argument);
  REQUIRE_THROWS_AS(forest.link(4, 0), std::invalid_argument);
  REQUIRE_THROWS_AS(forest.link(5, 5), std::invalid_argument);
  REQUIRE_THROWS_AS(forest.cut(0, 2), std::invalid_argument);

  forest.cut(2, 3);
  REQUIRE(!forest.connected(0, 4));
  REQUIRE(forest.connected(0, 2));
  REQUIRE(forest.connected(3, 4));
  REQUIRE_THROWS_AS(forest.cut(2, 3), std::invalid_argument);
  forest.link(2, 4);
  REQUIRE_EQ(forest.path_edge_distance(0, 3), static_cast<std::size_t>(4));
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_tree_repeated_evert_queries_preserve_topology) {
  LinkCutForest forest(9);
  const std::vector<std::pair<Vertex, Vertex>> edges = {
      {0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}, {5, 6}, {5, 7}, {7, 8}};
  for (const auto& [first, second] : edges) {
    forest.link(first, second);
  }

  for (std::size_t repeat = 0; repeat < 100; ++repeat) {
    REQUIRE_EQ(forest.path_edge_distance(3, 8), static_cast<std::size_t>(6));
    REQUIRE_EQ(forest.path_edge_distance(8, 3), static_cast<std::size_t>(6));
    REQUIRE_EQ(forest.path_edge_distance(4, 6), static_cast<std::size_t>(5));
    REQUIRE(forest.connected(6, 0));
    REQUIRE(forest.valid_auxiliary_invariants());
  }

  forest.cut(5, 7);
  REQUIRE(!forest.connected(8, 0));
  forest.link(4, 8);
  REQUIRE_EQ(forest.path_edge_distance(6, 3), static_cast<std::size_t>(5));
  REQUIRE_EQ(forest.path_edge_distance(6, 8), static_cast<std::size_t>(6));
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_tree_validates_vertex_bounds) {
  LinkCutForest empty(0);
  REQUIRE(empty.valid_auxiliary_invariants());
  REQUIRE_THROWS_AS(empty.connected(0, 0), std::out_of_range);

  LinkCutForest forest(3);
  REQUIRE_THROWS_AS(forest.link(0, 3), std::out_of_range);
  REQUIRE_THROWS_AS(forest.cut(3, 0), std::out_of_range);
  REQUIRE_THROWS_AS(forest.connected(0, 3), std::out_of_range);
  REQUIRE_THROWS_AS(forest.path_edge_distance(3, 0), std::out_of_range);
}

TEST_CASE(link_cut_tree_randomized_differential_against_naive_forest) {
  constexpr std::size_t kVertexCount = 36;
  constexpr std::size_t kOperations = 30000;
  LinkCutForest forest(kVertexCount);
  NaiveForest oracle(kVertexCount);
  std::mt19937_64 generator(0x1C7F0E57ULL);

  for (std::size_t operation = 0; operation < kOperations; ++operation) {
    const std::uint64_t choice = generator() % 100ULL;
    const Vertex first = random_vertex(generator, kVertexCount);
    const Vertex second = random_vertex(generator, kVertexCount);

    if (choice < 30ULL) {
      const bool can_link = first != second && !oracle.connected(first, second);
      if (can_link) {
        forest.link(first, second);
        oracle.link(first, second);
      } else {
        REQUIRE_THROWS_AS(forest.link(first, second), std::invalid_argument);
      }
    } else if (choice < 50ULL) {
      const auto current_edges = oracle.edges();
      const bool choose_existing =
          !current_edges.empty() && (generator() % 4ULL != 0ULL);
      Vertex cut_first = first;
      Vertex cut_second = second;
      if (choose_existing) {
        const std::size_t edge_index = static_cast<std::size_t>(
            generator() % static_cast<std::uint64_t>(current_edges.size()));
        cut_first = current_edges[edge_index].first;
        cut_second = current_edges[edge_index].second;
        if ((generator() & 1ULL) != 0ULL) {
          std::swap(cut_first, cut_second);
        }
      }

      if (cut_first != cut_second && oracle.has_edge(cut_first, cut_second)) {
        forest.cut(cut_first, cut_second);
        oracle.cut(cut_first, cut_second);
      } else {
        REQUIRE_THROWS_AS(forest.cut(cut_first, cut_second),
                          std::invalid_argument);
      }
    } else if (choice < 75ULL) {
      REQUIRE_EQ(forest.connected(first, second),
                 oracle.connected(first, second));
    } else {
      const auto expected = oracle.distance(first, second);
      if (expected.has_value()) {
        REQUIRE_EQ(forest.path_edge_distance(first, second), *expected);
      } else {
        REQUIRE_THROWS_AS(forest.path_edge_distance(first, second),
                          std::invalid_argument);
      }
    }

    REQUIRE(forest.valid_auxiliary_invariants());
    if (operation % 97 == 0) {
      for (Vertex vertex = 0; vertex < kVertexCount; ++vertex) {
        REQUIRE_EQ(forest.connected(vertex, vertex), true);
      }
    }
  }
}
