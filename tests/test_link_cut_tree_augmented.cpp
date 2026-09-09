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

  void assign_value(Vertex vertex, std::int64_t value) { values_[vertex] = value; }

  [[nodiscard]] std::optional<std::vector<Vertex>> path(Vertex first,
                                                       Vertex second) const {
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
      throw std::invalid_argument("naive path requires connected vertices");
    }
    return path_vertices->size() - 1;
  }

  [[nodiscard]] std::int64_t path_sum(Vertex first, Vertex second) const {
    const auto path_vertices = path(first, second);
    if (!path_vertices.has_value()) {
      throw std::invalid_argument("naive path requires connected vertices");
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
  return static_cast<Vertex>(generator() %
                             static_cast<std::uint64_t>(vertex_count));
}

std::int64_t random_small_value(std::mt19937_64& generator) {
  return static_cast<std::int64_t>(generator() % 2000001ULL) - 1000000;
}

}  // namespace

TEST_CASE(link_cut_augmented_assign_and_path_sum) {
  LinkCutForest forest(6);
  forest.assign_value(0, 5);
  forest.assign_value(1, -2);
  forest.assign_value(2, 7);
  forest.assign_value(3, 11);
  forest.assign_value(4, -4);
  forest.assign_value(5, 9);

  forest.link(0, 1);
  forest.link(1, 2);
  forest.link(2, 3);
  forest.link(2, 4);

  REQUIRE_EQ(forest.path_sum(0, 3), static_cast<std::int64_t>(21));
  REQUIRE_EQ(forest.path_sum(3, 0), static_cast<std::int64_t>(21));
  REQUIRE_EQ(forest.path_sum(4, 3), static_cast<std::int64_t>(14));
  REQUIRE_EQ(forest.path_sum(5, 5), static_cast<std::int64_t>(9));

  forest.assign_value(2, -7);
  REQUIRE_EQ(forest.path_sum(0, 3), static_cast<std::int64_t>(7));
  forest.cut(1, 2);
  REQUIRE_THROWS_AS(forest.path_sum(0, 3), std::invalid_argument);
  forest.link(1, 4);
  REQUIRE_EQ(forest.path_sum(0, 3), static_cast<std::int64_t>(3));
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_augmented_exact_cancellation_and_public_overflow) {
  LinkCutForest forest(5);
  const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();

  forest.assign_value(0, maximum);
  forest.assign_value(1, 1);
  forest.assign_value(2, -1);
  forest.assign_value(3, minimum);
  forest.assign_value(4, -1);
  forest.link(0, 1);
  forest.link(1, 2);
  forest.link(2, 3);
  forest.link(3, 4);

  REQUIRE_THROWS_AS(forest.path_sum(0, 1), std::overflow_error);
  REQUIRE(forest.valid_auxiliary_invariants());
  REQUIRE_EQ(forest.path_sum(0, 2), maximum);
  REQUIRE_EQ(forest.path_sum(0, 3), static_cast<std::int64_t>(-1));
  REQUIRE_EQ(forest.path_sum(3, 0), static_cast<std::int64_t>(-1));
  REQUIRE_THROWS_AS(forest.path_sum(3, 4), std::overflow_error);
  REQUIRE(forest.valid_auxiliary_invariants());

  forest.assign_value(1, 2);
  REQUIRE_EQ(forest.path_sum(0, 3), static_cast<std::int64_t>(0));
  REQUIRE_EQ(forest.path_sum(0, 4), static_cast<std::int64_t>(-1));
  REQUIRE_EQ(forest.path_sum(3, 3), minimum);
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_augmented_validation) {
  LinkCutForest empty(0);
  REQUIRE_THROWS_AS(empty.assign_value(0, 1), std::out_of_range);
  REQUIRE_THROWS_AS(empty.path_sum(0, 0), std::out_of_range);

  LinkCutForest forest(3);
  forest.assign_value(0, -9);
  REQUIRE_EQ(forest.path_sum(0, 0), static_cast<std::int64_t>(-9));
  REQUIRE_THROWS_AS(forest.path_sum(0, 2), std::invalid_argument);
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_augmented_randomized_against_naive_forest) {
  constexpr std::size_t kVertexCount = 40;
  constexpr std::size_t kOperations = 35000;
  LinkCutForest forest(kVertexCount);
  NaiveValuedForest oracle(kVertexCount);
  std::mt19937_64 generator(0xA6357A9EULL);

  for (std::size_t operation = 0; operation < kOperations; ++operation) {
    const std::uint64_t choice = generator() % 100ULL;
    const Vertex first = random_vertex(generator, kVertexCount);
    const Vertex second = random_vertex(generator, kVertexCount);

    if (choice < 20ULL) {
      const bool can_link = first != second && !oracle.connected(first, second);
      if (can_link) {
        forest.link(first, second);
        oracle.link(first, second);
      } else {
        REQUIRE_THROWS_AS(forest.link(first, second), std::invalid_argument);
      }
    } else if (choice < 35ULL) {
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
    } else if (choice < 60ULL) {
      const std::int64_t value = random_small_value(generator);
      forest.assign_value(first, value);
      oracle.assign_value(first, value);
      REQUIRE_EQ(forest.path_sum(first, first), value);
    } else if (choice < 75ULL) {
      REQUIRE_EQ(forest.connected(first, second),
                 oracle.connected(first, second));
    } else if (choice < 85ULL) {
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
