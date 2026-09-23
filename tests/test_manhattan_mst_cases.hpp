#pragma once

#include "algorithms/geometry/manhattan_mst.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/minimum_spanning_tree.hpp"

using algorithms::geometry::ManhattanPoint2D;
using algorithms::geometry::manhattan_minimum_spanning_tree;

namespace manhattan_mst_test_detail {

inline algorithms::graphs::Weight distance(
    const ManhattanPoint2D& left,
    const ManhattanPoint2D& right) {
  const std::int64_t dx =
      static_cast<std::int64_t>(left.x) -
      static_cast<std::int64_t>(right.x);
  const std::int64_t dy =
      static_cast<std::int64_t>(left.y) -
      static_cast<std::int64_t>(right.y);
  return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

inline algorithms::graphs::MinimumSpanningForest brute_complete_graph(
    const std::vector<ManhattanPoint2D>& points) {
  algorithms::graphs::Graph graph(points.size(), false);
  for (std::size_t left = 0U; left < points.size(); ++left) {
    for (std::size_t right = left + 1U;
         right < points.size(); ++right) {
      graph.add_edge(left, right, distance(points[left], points[right]));
    }
  }
  return algorithms::graphs::prim_minimum_spanning_forest(graph);
}

inline void require_tree_shape(
    const std::vector<ManhattanPoint2D>& points,
    const algorithms::graphs::MinimumSpanningForest& tree) {
  if (points.empty()) {
    REQUIRE_EQ(tree.component_count, 0U);
    REQUIRE(tree.edges.empty());
    REQUIRE_EQ(tree.total_weight, 0);
    return;
  }

  REQUIRE_EQ(tree.component_count, 1U);
  REQUIRE_EQ(tree.edges.size(), points.size() - 1U);

  std::vector<std::size_t> parent(points.size());
  std::vector<std::size_t> rank(points.size(), 0U);
  for (std::size_t vertex = 0U; vertex < points.size(); ++vertex) {
    parent[vertex] = vertex;
  }

  const auto find = [&parent](std::size_t vertex) {
    while (parent[vertex] != vertex) {
      parent[vertex] = parent[parent[vertex]];
      vertex = parent[vertex];
    }
    return vertex;
  };

  algorithms::graphs::Weight recomputed = 0;
  for (const auto& edge : tree.edges) {
    REQUIRE(edge.from < points.size());
    REQUIRE(edge.to < points.size());
    REQUIRE_EQ(
        edge.weight, distance(points[edge.from], points[edge.to]));

    std::size_t first = find(edge.from);
    std::size_t second = find(edge.to);
    REQUIRE(first != second);
    if (rank[first] < rank[second]) {
      std::swap(first, second);
    }
    parent[second] = first;
    if (rank[first] == rank[second]) {
      ++rank[first];
    }
    recomputed += edge.weight;
  }

  REQUIRE_EQ(recomputed, tree.total_weight);

  const std::size_t root = find(0U);
  for (std::size_t vertex = 1U; vertex < points.size(); ++vertex) {
    REQUIRE(find(vertex) == root);
  }
}

}  // namespace manhattan_mst_test_detail

TEST_CASE(manhattan_mst_empty_single_and_duplicates) {
  using namespace manhattan_mst_test_detail;

  const std::vector<ManhattanPoint2D> empty;
  const auto empty_tree = manhattan_minimum_spanning_tree(empty);
  require_tree_shape(empty, empty_tree);

  const std::vector<ManhattanPoint2D> single{{3, -4}};
  const auto single_tree = manhattan_minimum_spanning_tree(single);
  require_tree_shape(single, single_tree);
  REQUIRE_EQ(single_tree.total_weight, 0);

  const std::vector<ManhattanPoint2D> duplicates{
      {4, 9}, {4, 9}, {4, 9}, {10, 9}, {10, 9}};
  const auto duplicate_tree =
      manhattan_minimum_spanning_tree(duplicates);
  require_tree_shape(duplicates, duplicate_tree);
  REQUIRE_EQ(duplicate_tree.total_weight, 6);
  REQUIRE_EQ(
      duplicate_tree.total_weight,
      brute_complete_graph(duplicates).total_weight);
}

TEST_CASE(manhattan_mst_square_and_collinear_cases) {
  using namespace manhattan_mst_test_detail;

  const std::vector<ManhattanPoint2D> square{
      {0, 0}, {0, 10}, {10, 0}, {10, 10}};
  const auto square_tree = manhattan_minimum_spanning_tree(square);
  require_tree_shape(square, square_tree);
  REQUIRE_EQ(square_tree.total_weight, 30);
  REQUIRE_EQ(
      square_tree.total_weight,
      brute_complete_graph(square).total_weight);

  const std::vector<ManhattanPoint2D> line{
      {-10, 3}, {-4, 3}, {1, 3}, {20, 3}, {7, 3}};
  const auto line_tree = manhattan_minimum_spanning_tree(line);
  require_tree_shape(line, line_tree);
  REQUIRE_EQ(
      line_tree.total_weight,
      brute_complete_graph(line).total_weight);
}

TEST_CASE(manhattan_mst_full_int32_extremes) {
  using namespace manhattan_mst_test_detail;

  const auto minimum = std::numeric_limits<std::int32_t>::min();
  const auto maximum = std::numeric_limits<std::int32_t>::max();

  const std::vector<ManhattanPoint2D> points{
      {minimum, minimum},
      {maximum, maximum},
      {minimum, maximum},
      {maximum, minimum}};

  const auto tree = manhattan_minimum_spanning_tree(points);
  require_tree_shape(points, tree);
  REQUIRE_EQ(
      tree.total_weight,
      brute_complete_graph(points).total_weight);
}

TEST_CASE(manhattan_mst_random_matches_complete_graph_prim) {
  using namespace manhattan_mst_test_detail;

  std::mt19937_64 random(0x4D414E4841545441ULL);

  for (std::size_t trial = 0U; trial < 1600U; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(random() % 11U);
    std::vector<ManhattanPoint2D> points(size);

    for (auto& point : points) {
      point.x = static_cast<std::int32_t>(
          static_cast<std::int64_t>(random() % 31U) - 15);
      point.y = static_cast<std::int32_t>(
          static_cast<std::int64_t>(random() % 31U) - 15);
    }

    const auto sparse = manhattan_minimum_spanning_tree(points);
    const auto complete = brute_complete_graph(points);

    require_tree_shape(points, sparse);
    REQUIRE_EQ(sparse.total_weight, complete.total_weight);
  }
}
