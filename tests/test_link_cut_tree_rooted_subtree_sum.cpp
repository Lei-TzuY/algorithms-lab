#include "algorithms/graphs/link_cut_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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
  std::vector<Vertex> queue{start};
  parent[start] = start;
  for (std::size_t head = 0;
       head < queue.size() && parent[goal] == none; ++head) {
    const Vertex vertex = queue[head];
    for (Vertex next = 0; next < adjacency.size(); ++next) {
      if (adjacency[vertex][next] != 0U && parent[next] == none) {
        parent[next] = vertex;
        queue.push_back(next);
      }
    }
  }
  if (parent[goal] == none) {
    return {};
  }
  std::vector<Vertex> path;
  for (Vertex vertex = goal;; vertex = parent[vertex]) {
    path.push_back(vertex);
    if (vertex == start) {
      break;
    }
  }
  std::reverse(path.begin(), path.end());
  return path;
}

std::int64_t rooted_subtree_sum(const Adjacency& adjacency,
                                const std::vector<std::int64_t>& values,
                                Vertex root, Vertex vertex) {
  const Vertex none = static_cast<Vertex>(adjacency.size());
  std::vector<Vertex> parent(adjacency.size(), none);
  std::vector<Vertex> queue{root};
  parent[root] = root;
  for (std::size_t head = 0; head < queue.size(); ++head) {
    const Vertex current = queue[head];
    for (Vertex next = 0; next < adjacency.size(); ++next) {
      if (adjacency[current][next] != 0U && parent[next] == none) {
        parent[next] = current;
        queue.push_back(next);
      }
    }
  }
  if (parent[vertex] == none) {
    throw std::invalid_argument("disconnected rooted-subtree oracle");
  }

  std::int64_t sum = 0;
  std::vector<std::pair<Vertex, Vertex>> stack{{vertex, parent[vertex]}};
  while (!stack.empty()) {
    const auto [current, previous] = stack.back();
    stack.pop_back();
    sum += values[current];
    for (Vertex next = 0; next < adjacency.size(); ++next) {
      if (adjacency[current][next] != 0U && next != previous) {
        stack.emplace_back(next, current);
      }
    }
  }
  return sum;
}

std::vector<std::pair<Vertex, Vertex>> existing_edges(
    const Adjacency& adjacency) {
  std::vector<std::pair<Vertex, Vertex>> result;
  for (Vertex first = 0; first < adjacency.size(); ++first) {
    for (Vertex second = first + 1; second < adjacency.size(); ++second) {
      if (adjacency[first][second] != 0U) {
        result.emplace_back(first, second);
      }
    }
  }
  return result;
}

std::size_t pick(std::mt19937_64& engine, std::size_t bound) {
  return static_cast<std::size_t>(
      engine() % static_cast<std::uint64_t>(bound));
}

std::int64_t small_value(std::mt19937_64& engine) {
  return static_cast<std::int64_t>(engine() % 61U) - 30;
}

std::int64_t small_delta(std::mt19937_64& engine) {
  return static_cast<std::int64_t>(engine() % 9U) - 4;
}

std::int64_t path_sum(const std::vector<Vertex>& path,
                      const std::vector<std::int64_t>& values) {
  std::int64_t sum = 0;
  for (const Vertex vertex : path) {
    sum += values[vertex];
  }
  return sum;
}

}  // namespace

TEST_CASE(link_cut_rooted_subtree_sum_reroot_lazy_and_relink) {
  LinkCutForest forest(6);
  forest.link(0, 1);
  forest.link(1, 2);
  forest.link(1, 3);
  forest.link(3, 4);
  forest.link(3, 5);
  for (Vertex vertex = 0; vertex < 6; ++vertex) {
    forest.assign_value(vertex, static_cast<std::int64_t>(vertex + 1));
  }

  REQUIRE_EQ(forest.rooted_subtree_sum(0, 0), 21);
  REQUIRE_EQ(forest.rooted_subtree_sum(0, 1), 20);
  REQUIRE_EQ(forest.rooted_subtree_sum(0, 3), 15);
  REQUIRE_EQ(forest.rooted_subtree_sum(2, 1), 18);
  REQUIRE_EQ(forest.rooted_subtree_sum(5, 3), 15);

  forest.assign_path_value(2, 4, 7);
  REQUIRE_EQ(forest.rooted_subtree_sum(0, 1), 34);
  forest.add_path_value(0, 5, 3);
  REQUIRE_EQ(forest.rooted_subtree_sum(0, 1), 43);
  REQUIRE_EQ(forest.rooted_subtree_sum(2, 3), 26);

  forest.cut(1, 3);
  REQUIRE_THROWS_AS(forest.rooted_subtree_sum(0, 4), std::invalid_argument);
  forest.link(2, 4);
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_rooted_subtree_sum_exact_overflow_boundary) {
  LinkCutForest forest(2);
  forest.link(0, 1);
  forest.assign_value(0, std::numeric_limits<std::int64_t>::max());
  forest.assign_value(1, 1);
  REQUIRE_THROWS_AS(forest.rooted_subtree_sum(0, 0), std::overflow_error);
  REQUIRE_EQ(forest.rooted_subtree_sum(0, 1), 1);
  forest.assign_value(1, -1);
  REQUIRE_EQ(forest.rooted_subtree_sum(0, 0),
             std::numeric_limits<std::int64_t>::max() - 1);
  REQUIRE(forest.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_rooted_subtree_sum_randomized_against_eager_forest) {
  constexpr std::size_t kVertexCount = 12;
  constexpr std::size_t kSteps = 6000;
  LinkCutForest forest(kVertexCount);
  Adjacency adjacency(kVertexCount,
                      std::vector<unsigned char>(kVertexCount, 0U));
  std::vector<std::int64_t> values(kVertexCount, 0);
  std::mt19937_64 engine(0x40A66EULL);

  for (std::size_t step = 0; step < kSteps; ++step) {
    const Vertex first = pick(engine, kVertexCount);
    const Vertex second = pick(engine, kVertexCount);
    switch (pick(engine, 8)) {
      case 0:
        if (first != second && bfs_path(adjacency, first, second).empty()) {
          forest.link(first, second);
          adjacency[first][second] = 1U;
          adjacency[second][first] = 1U;
        }
        break;
      case 1: {
        const auto edges = existing_edges(adjacency);
        if (!edges.empty()) {
          const auto [edge_first, edge_second] =
              edges[pick(engine, edges.size())];
          forest.cut(edge_first, edge_second);
          adjacency[edge_first][edge_second] = 0U;
          adjacency[edge_second][edge_first] = 0U;
        }
        break;
      }
      case 2: {
        const std::int64_t value = small_value(engine);
        forest.assign_value(first, value);
        values[first] = value;
        break;
      }
      case 3: {
        const auto path = bfs_path(adjacency, first, second);
        if (!path.empty()) {
          const std::int64_t value = small_value(engine);
          forest.assign_path_value(first, second, value);
          for (const Vertex vertex : path) {
            values[vertex] = value;
          }
        }
        break;
      }
      case 4: {
        const auto path = bfs_path(adjacency, first, second);
        if (!path.empty()) {
          const std::int64_t delta = small_delta(engine);
          forest.add_path_value(first, second, delta);
          for (const Vertex vertex : path) {
            values[vertex] += delta;
          }
        }
        break;
      }
      case 5: {
        const auto path = bfs_path(adjacency, first, second);
        if (path.empty()) {
          REQUIRE_THROWS_AS(forest.rooted_subtree_sum(first, second),
                            std::invalid_argument);
        } else {
          REQUIRE_EQ(forest.rooted_subtree_sum(first, second),
                     rooted_subtree_sum(adjacency, values, first, second));
        }
        break;
      }
      case 6: {
        const auto path = bfs_path(adjacency, first, second);
        if (!path.empty()) {
          REQUIRE_EQ(forest.path_sum(first, second), path_sum(path, values));
        }
        break;
      }
      default:
        static_cast<void>(forest.connected(first, second));
        break;
    }

    if ((step % 31U) == 0U) {
      REQUIRE(forest.valid_auxiliary_invariants());
    }
  }

  for (Vertex root = 0; root < kVertexCount; ++root) {
    for (Vertex vertex = 0; vertex < kVertexCount; ++vertex) {
      const auto path = bfs_path(adjacency, root, vertex);
      if (path.empty()) {
        REQUIRE_THROWS_AS(forest.rooted_subtree_sum(root, vertex),
                          std::invalid_argument);
      } else {
        REQUIRE_EQ(forest.rooted_subtree_sum(root, vertex),
                   rooted_subtree_sum(adjacency, values, root, vertex));
      }
    }
  }
  REQUIRE(forest.valid_auxiliary_invariants());
}
