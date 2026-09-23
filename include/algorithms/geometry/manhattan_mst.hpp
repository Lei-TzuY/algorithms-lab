#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/minimum_spanning_tree.hpp"

namespace algorithms::geometry {

struct ManhattanPoint2D {
  std::int32_t x{};
  std::int32_t y{};

  friend bool operator==(const ManhattanPoint2D&,
                         const ManhattanPoint2D&) = default;
};

// Exact 2D Manhattan minimum spanning tree.
//
// The complete graph on n points has Theta(n^2) edges. This implementation
// first generates an O(n)-size exact candidate graph using four transformed
// octant sweeps, then delegates the sparse graph to the lab's existing
// Kruskal minimum-spanning-forest implementation.
//
// Duplicate coordinates are distinct vertices and are connected to one
// representative by explicit zero-weight edges before the geometric sweep.
//
// Coordinates are int32_t so every pairwise Manhattan distance fits exactly in
// the graph layer's signed int64_t Weight. The final total-weight overflow
// contract is inherited from kruskal_minimum_spanning_forest().
[[nodiscard]] inline graphs::MinimumSpanningForest
manhattan_minimum_spanning_tree(
    const std::vector<ManhattanPoint2D>& points) {
  using graphs::Graph;
  using graphs::Vertex;
  using graphs::Weight;

  const std::size_t n = points.size();
  Graph candidate_graph(n, false);

  if (n == 0U) {
    return graphs::kruskal_minimum_spanning_forest(candidate_graph);
  }

  const auto distance = [&points](const Vertex left,
                                  const Vertex right) -> Weight {
    const std::int64_t dx =
        static_cast<std::int64_t>(points[left].x) -
        static_cast<std::int64_t>(points[right].x);
    const std::int64_t dy =
        static_cast<std::int64_t>(points[left].y) -
        static_cast<std::int64_t>(points[right].y);
    const std::int64_t abs_dx = dx < 0 ? -dx : dx;
    const std::int64_t abs_dy = dy < 0 ? -dy : dy;
    return abs_dx + abs_dy;
  };

  std::vector<Vertex> by_coordinate(n);
  std::iota(by_coordinate.begin(), by_coordinate.end(), Vertex{0});
  std::sort(by_coordinate.begin(), by_coordinate.end(),
            [&points](const Vertex left, const Vertex right) {
              if (points[left].x != points[right].x) {
                return points[left].x < points[right].x;
              }
              if (points[left].y != points[right].y) {
                return points[left].y < points[right].y;
              }
              return left < right;
            });

  std::vector<Vertex> representatives;
  representatives.reserve(n);

  for (std::size_t begin = 0U; begin < n;) {
    const Vertex representative = by_coordinate[begin];
    representatives.push_back(representative);

    std::size_t end = begin + 1U;
    while (end < n &&
           points[by_coordinate[end]].x == points[representative].x &&
           points[by_coordinate[end]].y == points[representative].y) {
      candidate_graph.add_edge(representative, by_coordinate[end], 0);
      ++end;
    }
    begin = end;
  }

  std::vector<std::int64_t> x(n, 0);
  std::vector<std::int64_t> y(n, 0);
  for (const Vertex vertex : representatives) {
    x[vertex] = points[vertex].x;
    y[vertex] = points[vertex].y;
  }

  std::vector<Vertex> order = representatives;

  for (unsigned direction = 0U; direction < 4U; ++direction) {
    std::sort(order.begin(), order.end(),
              [&x, &y](const Vertex left, const Vertex right) {
                const std::int64_t left_sum = x[left] + y[left];
                const std::int64_t right_sum = x[right] + y[right];
                if (left_sum != right_sum) {
                  return left_sum < right_sum;
                }
                if (x[left] != x[right]) {
                  return x[left] < x[right];
                }
                if (y[left] != y[right]) {
                  return y[left] < y[right];
                }
                return left < right;
              });

    std::map<std::int64_t, Vertex> sweep;

    for (const Vertex vertex : order) {
      auto it = sweep.lower_bound(-y[vertex]);
      while (it != sweep.end()) {
        const Vertex other = it->second;
        if (x[vertex] - x[other] < y[vertex] - y[other]) {
          break;
        }

        candidate_graph.add_edge(
            vertex, other, distance(vertex, other));
        it = sweep.erase(it);
      }

      sweep[-y[vertex]] = vertex;
    }

    if ((direction & 1U) != 0U) {
      for (const Vertex vertex : representatives) {
        x[vertex] = -x[vertex];
      }
    } else {
      for (const Vertex vertex : representatives) {
        std::swap(x[vertex], y[vertex]);
      }
    }
  }

  graphs::MinimumSpanningForest result =
      graphs::kruskal_minimum_spanning_forest(candidate_graph);

  if (result.component_count != 1U ||
      result.edges.size() != n - 1U) {
    throw std::logic_error(
        "Manhattan MST candidate reduction failed to connect all points");
  }
  return result;
}

}  // namespace algorithms::geometry
