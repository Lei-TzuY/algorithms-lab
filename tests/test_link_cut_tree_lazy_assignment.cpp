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

class NaiveValuedForest {
 public:
  explicit NaiveValuedForest(std::size_t vertex_count)
      : adjacency_(vertex_count, std::vector<bool>(vertex_count, false)),
        values_(vertex_count, 0) {}

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return adjacency_.size();
  }

  [[nodiscard]] bool has_edge(Vertex first, Vertex second) const {
    return adjacency_[first][second];
  }

  void link(Vertex first, Vertex second) {
    adjacency_[first][second] = true;
    adjacency_[second][first] = true;
  }

  void cut(Vertex first, Vertex second) {
    adjacency_[first][second] = false;
    adjacency_[second][first] = false;
  }

  void assign_value(Vertex vertex, std::int64_t value) {
    values_[vertex] = value;
  }

  [[nodiscard]] std::optional<std::vector<Vertex>> path(
      Vertex first, Vertex second) const {
    constexpr Vertex kNone = std::numeric_limits<Vertex>::max();
    std::vector<Vertex> parent(vertex_count(), kNone);
    std::queue<Vertex> queue;
    parent[first] = first;
    queue.push(first);
    while (!queue.empty()) {
      const Vertex vertex = queue.front();
      queue.pop();
      if (vertex == second) {
        break;
      }
      for (Vertex neighbor = 0; neighbor < vertex_count(); ++neighbor) {
        if (adjacency_[vertex][neighbor] && parent[neighbor] == kNone) {
          parent[neighbor] = vertex;
          queue.push(neighbor);
        }
      }
    }
    if (parent[second] == kNone) {
      return std::nullopt;
    }
    std::vector<Vertex> result;
    for (Vertex current = second;; current = parent[current]) {
      result.push_back(current);
      if (current == first) {
        break;
      }
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

  [[nodiscard]] bool connected(Vertex first, Vertex second) const {
    return path(first, second).has_value();
  }

  [[nodiscard]] std::size_t distance(Vertex first, Vertex second) const {
    const auto path_vertices = path(first, second);
    if (!path_vertices.has_value()) {
      throw std::invalid_argument("disconnected");
    }
    return path_vertices->size() - 1;
  }

  void assign_path(Vertex first, Vertex second, std::int64_t value) {
    const auto path_vertices = path(first, second);
    if (!path_vertices.has_value()) {
      throw std::invalid_argument("disconnected");
    }
    for (const Vertex vertex : *path_vertices) {
      values_[vertex] = value;
    }
  }

  [[nodiscard]] std::int64_t path_sum(Vertex first, Vertex second) const {
    const auto path_vertices = path(first, second);
    if (!path_vertices.has_value()) {
      throw std::invalid_argument("disconnected");
    }
    std::int64_t sum = 0;
    for (const Vertex vertex : *path_vertices) {
      sum += values_[vertex];
    }
    return sum;
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
  std::vector<std::int64_t> values_;
};

Vertex random_vertex(std::mt19937_64& generator, std::size_t vertex_count) {
  return static_cast<Vertex>(
      generator() % static_cast<std::uint64_t>(vertex_count));
}

std::int64_t random_small_value(std::mt19937_64& generator) {
  return static_cast<std::int64_t>(generator() % 2000001ULL) - 1000000;
}

}  // namespace

TEST_CASE(link_cut_path_assignment_deferred_overwrite_and_topology) {
  LinkCutForest forest(6);
  for (Vertex vertex = 0; vertex < 6; ++vertex) {
    forest.assign_value(vertex, static_cast<std::int64_t>(vertex + 1));
  }
  for (Vertex vertex = 0; vertex + 1 < 6; ++vertex) {
    forest.link(vertex, vertex + 1);
  }

  forest.assign_path_value(1, 4, 7);
  REQUIRE(forest.valid_auxiliary_invariants());
  REQUIRE_EQ(forest.path_sum(0, 5), static_cast<std::int64_t>(35));
  REQUIRE_EQ(forest.path_sum(5, 0), static_cast<std::int64_t>(35));

  forest.assign_path_value(2, 5, -3);
  REQUIRE(forest.valid_auxiliary_invariants());
  REQUIRE_EQ(forest.path_sum(0, 5), static_cast<std::int64_t>(-4));

  forest.assign_value(3, 11);
  REQUIRE_EQ(forest.path_sum(2, 4), static_cast<std::int64_t>(5));
  REQUIRE(forest.valid_auxiliary_invariants());

  forest.cut(3, 4);
  REQUIRE_EQ(forest.path_sum(0, 3), static_cast<std::int64_t>(16));
  REQUIRE_EQ(forest.path_sum(4, 5), static_cast<std::int64_t>(-6));
  forest.link(0, 5);
  REQUIRE_EQ(forest.path_sum(3, 4), static_cast<std::int64_t>(10));
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_path_assignment_overflow_and_cancellation) {
  LinkCutForest forest(3);
  forest.link(0, 1);
  forest.link(1, 2);
  const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();

  forest.assign_path_value(0, 2, maximum);
  REQUIRE(forest.valid_auxiliary_invariants());
  REQUIRE_THROWS_AS(forest.path_sum(0, 2), std::overflow_error);
  forest.assign_value(1, -maximum);
  REQUIRE_EQ(forest.path_sum(0, 2), maximum);
  REQUIRE(forest.valid_auxiliary_invariants());

  forest.assign_path_value(0, 1, minimum);
  REQUIRE_THROWS_AS(forest.path_sum(0, 1), std::overflow_error);
  REQUIRE_EQ(forest.path_sum(1, 2), static_cast<std::int64_t>(-1));
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_path_assignment_validation_and_singleton) {
  LinkCutForest forest(4);
  forest.assign_value(0, 9);
  forest.assign_value(1, 8);
  forest.link(0, 1);

  // Regression: access(0) alone can retain another preferred-path vertex in
  // the same auxiliary subtree. A singleton assignment must still expose the
  // represented 0..0 path before applying the lazy tag.
  forest.assign_path_value(0, 0, -12);
  REQUIRE_EQ(forest.path_sum(0, 0), static_cast<std::int64_t>(-12));
  REQUIRE_THROWS_AS(forest.assign_path_value(0, 2, 7), std::invalid_argument);
  REQUIRE_EQ(forest.path_sum(0, 1), static_cast<std::int64_t>(-4));
  REQUIRE_THROWS_AS(forest.assign_path_value(0, 4, 1), std::out_of_range);
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_path_assignment_randomized_against_naive_forest) {
  constexpr std::size_t kVertexCount = 40;
  constexpr std::size_t kOperations = 30000;
  LinkCutForest forest(kVertexCount);
  NaiveValuedForest oracle(kVertexCount);
  std::mt19937_64 generator(0x36A5516EULL);

  for (std::size_t operation = 0; operation < kOperations; ++operation) {
    const std::uint64_t choice = generator() % 100ULL;
    const Vertex first = random_vertex(generator, kVertexCount);
    const Vertex second = random_vertex(generator, kVertexCount);

    if (choice < 15ULL) {
      const bool can_link =
          first != second && !oracle.connected(first, second);
      if (can_link) {
        forest.link(first, second);
        oracle.link(first, second);
      } else {
        REQUIRE_THROWS_AS(forest.link(first, second), std::invalid_argument);
      }
    } else if (choice < 27ULL) {
      const auto edges = oracle.edges();
      const bool choose_existing =
          !edges.empty() && (generator() % 4ULL != 0ULL);
      Vertex cut_first = first;
      Vertex cut_second = second;
      if (choose_existing) {
        const auto& edge = edges[static_cast<std::size_t>(
            generator() % static_cast<std::uint64_t>(edges.size()))];
        cut_first = edge.first;
        cut_second = edge.second;
        if ((generator() & 1ULL) != 0ULL) {
          std::swap(cut_first, cut_second);
        }
      }
      if (cut_first != cut_second &&
          oracle.has_edge(cut_first, cut_second)) {
        forest.cut(cut_first, cut_second);
        oracle.cut(cut_first, cut_second);
      } else {
        REQUIRE_THROWS_AS(forest.cut(cut_first, cut_second),
                          std::invalid_argument);
      }
    } else if (choice < 44ULL) {
      const std::int64_t value = random_small_value(generator);
      forest.assign_value(first, value);
      oracle.assign_value(first, value);
    } else if (choice < 64ULL) {
      const std::int64_t value = random_small_value(generator);
      if (oracle.connected(first, second)) {
        forest.assign_path_value(first, second, value);
        oracle.assign_path(first, second, value);
      } else {
        REQUIRE_THROWS_AS(
            forest.assign_path_value(first, second, value),
            std::invalid_argument);
      }
    } else if (choice < 75ULL) {
      REQUIRE_EQ(forest.connected(first, second),
                 oracle.connected(first, second));
    } else if (choice < 84ULL) {
      if (oracle.connected(first, second)) {
        REQUIRE_EQ(forest.path_edge_distance(first, second),
                   oracle.distance(first, second));
      } else {
        REQUIRE_THROWS_AS(forest.path_edge_distance(first, second),
                          std::invalid_argument);
      }
    } else {
      if (oracle.connected(first, second)) {
        REQUIRE_EQ(forest.path_sum(first, second),
                   oracle.path_sum(first, second));
      } else {
        REQUIRE_THROWS_AS(forest.path_sum(first, second),
                          std::invalid_argument);
      }
    }

    REQUIRE(forest.valid_auxiliary_invariants());
  }
}
