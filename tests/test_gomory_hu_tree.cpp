#include "algorithms/graphs/gomory_hu_tree.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "test_framework.hpp"

using algorithms::graphs::Capacity;
using algorithms::graphs::UndirectedCapacityEdge;
using algorithms::graphs::Vertex;
using algorithms::graphs::build_gomory_hu_tree;

namespace {

Capacity exhaustive_min_cut(std::size_t vertex_count,
                            const std::vector<UndirectedCapacityEdge>& edges,
                            Vertex source, Vertex sink) {
  if (vertex_count > 62) {
    throw std::logic_error("test oracle supports at most 62 vertices");
  }
  Capacity best = std::numeric_limits<Capacity>::max();
  const std::uint64_t total_masks = std::uint64_t{1} << vertex_count;
  for (std::uint64_t mask = 0; mask < total_masks; ++mask) {
    const bool source_in = ((mask >> source) & std::uint64_t{1}) != 0;
    const bool sink_in = ((mask >> sink) & std::uint64_t{1}) != 0;
    if (!source_in || sink_in) {
      continue;
    }
    Capacity cut = 0;
    for (const auto& edge : edges) {
      if (edge.first == edge.second) {
        continue;
      }
      const bool first_in = ((mask >> edge.first) & std::uint64_t{1}) != 0;
      const bool second_in = ((mask >> edge.second) & std::uint64_t{1}) != 0;
      if (first_in != second_in) {
        cut += edge.capacity;
      }
    }
    best = std::min(best, cut);
  }
  return best;
}

void verify_against_exhaustive_oracle(
    std::size_t vertex_count,
    const std::vector<UndirectedCapacityEdge>& edges) {
  const auto tree = build_gomory_hu_tree(vertex_count, edges);
  REQUIRE_EQ(tree.vertex_count(), vertex_count);
  REQUIRE_EQ(tree.parent().size(), vertex_count);
  REQUIRE_EQ(tree.cut_to_parent().size(), vertex_count);

  if (vertex_count > 0) {
    REQUIRE_EQ(tree.parent()[0], Vertex{0});
    REQUIRE_EQ(tree.cut_to_parent()[0], Capacity{0});
    for (Vertex vertex = 1; vertex < vertex_count; ++vertex) {
      REQUIRE(tree.parent()[vertex] < vertex_count);
      REQUIRE_EQ(tree.min_cut(vertex, tree.parent()[vertex]),
                 tree.cut_to_parent()[vertex]);
    }
  }

  for (Vertex first = 0; first < vertex_count; ++first) {
    for (Vertex second = first + 1; second < vertex_count; ++second) {
      REQUIRE_EQ(tree.min_cut(first, second),
                 exhaustive_min_cut(vertex_count, edges, first, second));
    }
  }
}

}  // namespace

TEST_CASE(gomory_hu_empty_singleton_and_validation) {
  const std::vector<UndirectedCapacityEdge> empty_edges;
  const auto empty_tree = build_gomory_hu_tree(0, empty_edges);
  REQUIRE(empty_tree.parent().empty());
  REQUIRE(empty_tree.cut_to_parent().empty());
  REQUIRE_THROWS_AS(empty_tree.min_cut(0, 1), std::out_of_range);

  const std::vector<UndirectedCapacityEdge> singleton_edges = {{0, 0, 99}};
  const auto singleton = build_gomory_hu_tree(1, singleton_edges);
  REQUIRE_EQ(singleton.parent().size(), std::size_t{1});
  REQUIRE_EQ(singleton.parent()[0], Vertex{0});
  REQUIRE_EQ(singleton.cut_to_parent()[0], Capacity{0});
  REQUIRE_THROWS_AS(singleton.min_cut(0, 0), std::invalid_argument);

  const std::vector<UndirectedCapacityEdge> negative = {{0, 1, -1}};
  REQUIRE_THROWS_AS(build_gomory_hu_tree(2, negative), std::invalid_argument);

  const std::vector<UndirectedCapacityEdge> bad_endpoint = {{0, 2, 1}};
  REQUIRE_THROWS_AS(build_gomory_hu_tree(2, bad_endpoint), std::out_of_range);
}

TEST_CASE(gomory_hu_parallel_self_loop_zero_and_determinism) {
  const std::vector<UndirectedCapacityEdge> edges = {
      {0, 1, 3}, {1, 2, 4}, {2, 3, 5}, {3, 0, 6},
      {0, 2, 2}, {0, 1, 7}, {1, 1, 1000}, {2, 3, 0},
  };
  verify_against_exhaustive_oracle(4, edges);

  const auto first = build_gomory_hu_tree(4, edges);
  const auto second = build_gomory_hu_tree(4, edges);
  REQUIRE(first.parent() == second.parent());
  REQUIRE(first.cut_to_parent() == second.cut_to_parent());
}

TEST_CASE(gomory_hu_disconnected_and_overflow) {
  const std::vector<UndirectedCapacityEdge> disconnected = {{0, 1, 8}};
  verify_against_exhaustive_oracle(4, disconnected);
  const auto tree = build_gomory_hu_tree(4, disconnected);
  REQUIRE_EQ(tree.min_cut(0, 2), Capacity{0});
  REQUIRE_EQ(tree.min_cut(2, 3), Capacity{0});

  const Capacity maximum = std::numeric_limits<Capacity>::max();
  const std::vector<UndirectedCapacityEdge> overflow = {
      {0, 1, maximum}, {0, 1, 1}};
  REQUIRE_THROWS_AS(build_gomory_hu_tree(2, overflow), std::overflow_error);
}

TEST_CASE(gomory_hu_randomized_all_pairs_differential) {
  std::mt19937_64 rng(0x474F4D4F52594855ULL);
  for (std::size_t trial = 0; trial < 400; ++trial) {
    const std::size_t vertex_count = 2 + static_cast<std::size_t>(rng() % 7U);
    const std::size_t edge_count = static_cast<std::size_t>(rng() % 22U);
    std::vector<UndirectedCapacityEdge> edges;
    edges.reserve(edge_count);
    for (std::size_t edge = 0; edge < edge_count; ++edge) {
      const Vertex first = static_cast<Vertex>(rng() % vertex_count);
      const Vertex second = static_cast<Vertex>(rng() % vertex_count);
      const Capacity capacity = static_cast<Capacity>(rng() % 21U);
      edges.push_back({first, second, capacity});
    }
    verify_against_exhaustive_oracle(vertex_count, edges);
  }
}
