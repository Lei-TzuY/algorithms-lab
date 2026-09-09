#include "algorithms/graphs/link_cut_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::LinkCutForest;
using algorithms::graphs::Vertex;
using Adjacency = std::vector<std::vector<unsigned char>>;

std::vector<Vertex> bfs_path(const Adjacency& adjacency, Vertex start,
                             Vertex goal) {
  const Vertex none = static_cast<Vertex>(adjacency.size());
  std::vector<Vertex> parent(adjacency.size(), none);
  std::vector<Vertex> queue;
  parent[start] = start;
  queue.push_back(start);

  for (std::size_t head = 0;
       head < queue.size() && parent[goal] == none; ++head) {
    const Vertex vertex = queue[head];
    for (Vertex next = 0; next < adjacency.size(); ++next) {
      if (adjacency[vertex][next] == 0U || parent[next] != none) {
        continue;
      }
      parent[next] = vertex;
      queue.push_back(next);
    }
  }

  if (parent[goal] == none) {
    return {};
  }

  std::vector<Vertex> path;
  for (Vertex current = goal;; current = parent[current]) {
    path.push_back(current);
    if (current == start) {
      break;
    }
  }
  std::reverse(path.begin(), path.end());
  return path;
}

std::size_t pick(std::mt19937_64& engine, std::size_t bound) {
  return static_cast<std::size_t>(
      engine() % static_cast<std::uint64_t>(bound));
}

std::int64_t small_value(std::mt19937_64& engine) {
  return static_cast<std::int64_t>(engine() % 51U) - 25;
}

std::int64_t small_delta(std::mt19937_64& engine) {
  return static_cast<std::int64_t>(engine() % 7U) - 3;
}

std::vector<std::pair<Vertex, Vertex>> existing_edges(
    const Adjacency& adjacency) {
  std::vector<std::pair<Vertex, Vertex>> edges;
  for (Vertex first = 0; first < adjacency.size(); ++first) {
    for (Vertex second = first + 1; second < adjacency.size(); ++second) {
      if (adjacency[first][second] != 0U) {
        edges.emplace_back(first, second);
      }
    }
  }
  return edges;
}

std::int64_t path_value_sum(const std::vector<Vertex>& path,
                            const std::vector<std::int64_t>& values) {
  std::int64_t sum = 0;
  for (const Vertex vertex : path) {
    sum += values[vertex];
  }
  return sum;
}

}  // namespace

TEST_CASE(link_cut_ordered_navigation_forward_reverse_bounds_and_lazy_tags) {
  LinkCutForest forest(6);
  for (Vertex vertex = 0; vertex + 1 < 5; ++vertex) {
    forest.link(vertex, vertex + 1);
  }

  forest.assign_path_value(0, 4, 10);
  forest.add_path_value(1, 3, 5);

  for (std::size_t rank = 0; rank < 5; ++rank) {
    REQUIRE_EQ(forest.kth_vertex_on_path(0, 4, rank), rank);
    REQUIRE_EQ(forest.kth_vertex_on_path(4, 0, rank), 4 - rank);
  }

  REQUIRE_EQ(forest.kth_vertex_on_path(2, 2, 0), 2U);
  REQUIRE_THROWS_AS(forest.kth_vertex_on_path(2, 2, 1), std::out_of_range);
  REQUIRE_THROWS_AS(forest.kth_vertex_on_path(0, 4, 5), std::out_of_range);
  REQUIRE_THROWS_AS(forest.kth_vertex_on_path(0, 5, 0),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(forest.kth_vertex_on_path(6, 0, 0), std::out_of_range);

  REQUIRE_EQ(forest.path_sum(0, 4), 65);
  REQUIRE_EQ(forest.path_sum(1, 3), 45);
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_ordered_navigation_survives_cut_relink_and_pending_tags) {
  LinkCutForest forest(5);
  forest.link(0, 1);
  forest.link(1, 2);
  forest.link(1, 3);
  forest.link(3, 4);

  const std::vector<Vertex> first_path{2, 1, 3, 4};
  for (std::size_t rank = 0; rank < first_path.size(); ++rank) {
    REQUIRE_EQ(forest.kth_vertex_on_path(2, 4, rank), first_path[rank]);
  }

  forest.cut(1, 3);
  REQUIRE_THROWS_AS(forest.kth_vertex_on_path(0, 4, 0),
                    std::invalid_argument);
  forest.link(2, 4);

  forest.assign_path_value(0, 3, 4);
  forest.add_path_value(1, 4, 2);
  const std::vector<Vertex> relinked_path{0, 1, 2, 4, 3};
  for (std::size_t rank = 0; rank < relinked_path.size(); ++rank) {
    REQUIRE_EQ(forest.kth_vertex_on_path(0, 3, rank), relinked_path[rank]);
  }
  REQUIRE_EQ(forest.path_sum(0, 3), 26);
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_ordered_navigation_randomized_against_bfs_path_oracle) {
  constexpr std::size_t kVertexCount = 12;
  constexpr std::size_t kSteps = 3500;
  LinkCutForest forest(kVertexCount);
  Adjacency adjacency(kVertexCount,
                      std::vector<unsigned char>(kVertexCount, 0U));
  std::vector<std::int64_t> values(kVertexCount, 0);
  std::mt19937_64 engine(0x38A11CEULL);

  for (std::size_t step = 0; step < kSteps; ++step) {
    const Vertex first = pick(engine, kVertexCount);
    const Vertex second = pick(engine, kVertexCount);
    const std::size_t operation = pick(engine, 7);

    if (operation == 0) {
      if (first != second && bfs_path(adjacency, first, second).empty()) {
        forest.link(first, second);
        adjacency[first][second] = 1U;
        adjacency[second][first] = 1U;
      }
    } else if (operation == 1) {
      const auto edges = existing_edges(adjacency);
      if (!edges.empty()) {
        const auto [edge_first, edge_second] =
            edges[pick(engine, edges.size())];
        forest.cut(edge_first, edge_second);
        adjacency[edge_first][edge_second] = 0U;
        adjacency[edge_second][edge_first] = 0U;
      }
    } else if (operation == 2) {
      const std::int64_t value = small_value(engine);
      forest.assign_value(first, value);
      values[first] = value;
    } else if (operation == 3) {
      const auto path = bfs_path(adjacency, first, second);
      if (!path.empty()) {
        const std::int64_t value = small_value(engine);
        forest.assign_path_value(first, second, value);
        for (const Vertex vertex : path) {
          values[vertex] = value;
        }
      }
    } else if (operation == 4) {
      const auto path = bfs_path(adjacency, first, second);
      if (!path.empty()) {
        const std::int64_t delta = small_delta(engine);
        forest.add_path_value(first, second, delta);
        for (const Vertex vertex : path) {
          values[vertex] += delta;
        }
      }
    } else if (operation == 5) {
      const auto path = bfs_path(adjacency, first, second);
      if (path.empty()) {
        REQUIRE_THROWS_AS(forest.kth_vertex_on_path(first, second, 0),
                          std::invalid_argument);
      } else {
        const std::size_t rank = pick(engine, path.size());
        REQUIRE_EQ(forest.kth_vertex_on_path(first, second, rank), path[rank]);
        if ((step % 17U) == 0U) {
          REQUIRE_THROWS_AS(
              forest.kth_vertex_on_path(first, second, path.size()),
              std::out_of_range);
        }
      }
    } else {
      const auto path = bfs_path(adjacency, first, second);
      if (path.empty()) {
        REQUIRE_THROWS_AS(forest.path_sum(first, second),
                          std::invalid_argument);
      } else {
        REQUIRE_EQ(forest.path_sum(first, second),
                   path_value_sum(path, values));
      }
    }

    if ((step % 41U) == 0U) {
      REQUIRE(forest.valid_auxiliary_invariants());
    }
  }

  for (Vertex first = 0; first < kVertexCount; ++first) {
    for (Vertex second = 0; second < kVertexCount; ++second) {
      const auto path = bfs_path(adjacency, first, second);
      if (path.empty()) {
        REQUIRE_THROWS_AS(forest.kth_vertex_on_path(first, second, 0),
                          std::invalid_argument);
        continue;
      }
      for (std::size_t rank = 0; rank < path.size(); ++rank) {
        REQUIRE_EQ(forest.kth_vertex_on_path(first, second, rank), path[rank]);
      }
      REQUIRE_EQ(forest.path_sum(first, second), path_value_sum(path, values));
    }
  }
  REQUIRE(forest.valid_auxiliary_invariants());
}
