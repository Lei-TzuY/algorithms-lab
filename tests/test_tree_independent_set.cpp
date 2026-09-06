#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/dynamic_programming/tree_independent_set.hpp"
#include "algorithms/graphs/graph.hpp"

namespace {

using algorithms::dynamic_programming::TreeIndependentSetResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

void require_valid_independent_set(
    const Graph& tree, const std::vector<std::int64_t>& weights,
    const TreeIndependentSetResult& result) {
  REQUIRE_EQ(weights.size(), tree.vertex_count());
  std::vector<bool> selected(tree.vertex_count(), false);
  std::int64_t total = 0;
  for (const Vertex vertex : result.vertices) {
    REQUIRE(vertex < tree.vertex_count());
    REQUIRE(!selected[vertex]);
    selected[vertex] = true;
    total += weights[vertex];
  }

  for (Vertex vertex = 0; vertex < tree.vertex_count(); ++vertex) {
    if (!selected[vertex]) {
      continue;
    }
    for (const auto& edge : tree.neighbors(vertex)) {
      REQUIRE(!selected[edge.to]);
    }
  }
  REQUIRE_EQ(total, result.maximum_weight);
}

std::int64_t exhaustive_independent_set_weight(
    std::size_t vertex_count, const std::vector<std::int64_t>& weights,
    const std::vector<std::pair<Vertex, Vertex>>& edges) {
  REQUIRE(vertex_count <= 20U);
  const std::uint64_t limit = std::uint64_t{1} << vertex_count;
  std::int64_t best = 0;

  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    bool independent = true;
    for (const auto& edge : edges) {
      const bool left = (mask & (std::uint64_t{1} << edge.first)) != 0U;
      const bool right = (mask & (std::uint64_t{1} << edge.second)) != 0U;
      if (left && right) {
        independent = false;
        break;
      }
    }
    if (!independent) {
      continue;
    }

    std::int64_t total = 0;
    for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
      if ((mask & (std::uint64_t{1} << vertex)) != 0U) {
        total += weights[vertex];
      }
    }
    if (total > best) {
      best = total;
    }
  }
  return best;
}

}  // namespace

TEST_CASE(tree_independent_set_handles_empty_single_negative_and_ties) {
  const Graph empty(0, false);
  const auto empty_result =
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          empty, {});
  REQUIRE_EQ(empty_result.maximum_weight, std::int64_t{0});
  REQUIRE(empty_result.vertices.empty());

  const Graph singleton(1, false);
  const auto positive =
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          singleton, {9});
  REQUIRE_EQ(positive.maximum_weight, std::int64_t{9});
  REQUIRE_EQ(positive.vertices, (std::vector<Vertex>{0}));

  const auto negative =
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          singleton, {-9});
  REQUIRE_EQ(negative.maximum_weight, std::int64_t{0});
  REQUIRE(negative.vertices.empty());

  Graph tie(2, false);
  tie.add_edge(0, 1, 77);
  const auto tied =
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          tie, {5, 5}, 0);
  REQUIRE_EQ(tied.maximum_weight, std::int64_t{5});
  REQUIRE_EQ(tied.vertices, (std::vector<Vertex>{1}));
}

TEST_CASE(tree_independent_set_reconstructs_known_optimum) {
  Graph tree(5, false);
  tree.add_edge(0, 1, 91);
  tree.add_edge(0, 2, -4);
  tree.add_edge(1, 3, 700);
  tree.add_edge(1, 4, 3);
  const std::vector<std::int64_t> weights{5, 1, 4, 10, 10};

  const auto result =
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          tree, weights, 0);
  REQUIRE_EQ(result.maximum_weight, std::int64_t{25});
  REQUIRE_EQ(result.vertices, (std::vector<Vertex>{0, 3, 4}));
  require_valid_independent_set(tree, weights, result);

  Graph chain(3, false);
  chain.add_edge(0, 1);
  chain.add_edge(1, 2);
  const auto all_negative =
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          chain, {-5, -1, -3}, 2);
  REQUIRE_EQ(all_negative.maximum_weight, std::int64_t{0});
  REQUIRE(all_negative.vertices.empty());
}

TEST_CASE(tree_independent_set_rejects_non_tree_inputs_and_overflow) {
  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          directed, {1, 2}),
      std::invalid_argument);

  const Graph mismatch(1, false);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          mismatch, {}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          mismatch, {1}, 1),
      std::out_of_range);

  Graph disconnected(3, false);
  disconnected.add_edge(0, 1);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          disconnected, {1, 2, 3}),
      std::invalid_argument);

  Graph cyclic(3, false);
  cyclic.add_edge(0, 1);
  cyclic.add_edge(1, 2);
  cyclic.add_edge(2, 0);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          cyclic, {1, 2, 3}),
      std::invalid_argument);

  Graph self_loop(1, false);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          self_loop, {1}),
      std::invalid_argument);

  Graph parallel(2, false);
  parallel.add_edge(0, 1);
  parallel.add_edge(0, 1);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          parallel, {1, 2}),
      std::invalid_argument);

  Graph overflow(3, false);
  overflow.add_edge(0, 1);
  overflow.add_edge(0, 2);
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::maximum_weight_independent_set_tree(
          overflow,
          {0, std::numeric_limits<std::int64_t>::max(),
           std::numeric_limits<std::int64_t>::max()}),
      std::overflow_error);
}

TEST_CASE(tree_independent_set_matches_exhaustive_randomized_oracle) {
  std::mt19937_64 rng(0x545245454450ULL);
  std::uniform_int_distribution<int> count_distribution(0, 12);
  std::uniform_int_distribution<int> weight_distribution(-8, 20);

  for (std::size_t trial = 0; trial < 360; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(count_distribution(rng));
    Graph tree(vertex_count, false);
    std::vector<std::pair<Vertex, Vertex>> edges;
    edges.reserve(vertex_count == 0U ? 0U : vertex_count - 1U);

    for (Vertex vertex = 1; vertex < vertex_count; ++vertex) {
      std::uniform_int_distribution<std::size_t> parent_distribution(
          0U, vertex - 1U);
      const Vertex parent = parent_distribution(rng);
      tree.add_edge(parent, vertex,
                    static_cast<std::int64_t>(vertex * 17U + 3U));
      edges.emplace_back(parent, vertex);
    }

    std::vector<std::int64_t> weights;
    weights.reserve(vertex_count);
    for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
      weights.push_back(static_cast<std::int64_t>(weight_distribution(rng)));
    }

    const Vertex root = vertex_count == 0U
                            ? Vertex{0}
                            : std::uniform_int_distribution<std::size_t>(
                                  0U, vertex_count - 1U)(rng);
    const auto result =
        algorithms::dynamic_programming::maximum_weight_independent_set_tree(
            tree, weights, root);
    const std::int64_t oracle =
        exhaustive_independent_set_weight(vertex_count, weights, edges);
    REQUIRE_EQ(result.maximum_weight, oracle);
    require_valid_independent_set(tree, weights, result);
  }
}
