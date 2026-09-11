#include "algorithms/graphs/chinese_postman.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

using algorithms::graphs::ChinesePostmanResult;
using algorithms::graphs::Graph;
using algorithms::graphs::PostmanEdgeReference;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;
using algorithms::graphs::minimum_chinese_postman_tour;

namespace {

struct OracleEdge {
  Vertex first = 0;
  Vertex second = 0;
  Weight weight = 0;
  std::size_t adjacency_index = 0;
};

std::vector<OracleEdge> logical_edges(const Graph& graph) {
  std::vector<OracleEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& neighbors = graph.neighbors(from);
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
      const auto& edge = neighbors[index];
      if (from <= edge.to) {
        edges.push_back({from, edge.to, edge.weight, index});
      }
    }
  }
  return edges;
}

std::optional<std::uint64_t> exact_closed_walk_oracle(const Graph& graph) {
  const auto edges = logical_edges(graph);
  if (edges.empty()) {
    return std::uint64_t{0};
  }
  if (edges.size() >= std::numeric_limits<std::size_t>::digits) {
    throw std::runtime_error("oracle edge mask too large");
  }
  std::vector<std::vector<std::size_t>> incident(graph.vertex_count());
  for (std::size_t id = 0; id < edges.size(); ++id) {
    incident[edges[id].first].push_back(id);
    if (edges[id].first != edges[id].second) {
      incident[edges[id].second].push_back(id);
    }
  }
  Vertex start = 0;
  while (start < graph.vertex_count() && incident[start].empty()) {
    ++start;
  }
  REQUIRE(start < graph.vertex_count());

  const std::size_t mask_count = std::size_t{1} << edges.size();
  const auto index_of = [count = graph.vertex_count()](std::size_t mask,
                                                       Vertex vertex) {
    return mask * count + vertex;
  };
  constexpr std::uint64_t inf = std::numeric_limits<std::uint64_t>::max();
  std::vector<std::uint64_t> distance(mask_count * graph.vertex_count(), inf);
  using Item = std::tuple<std::uint64_t, std::size_t, Vertex>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
  distance[index_of(0, start)] = 0;
  queue.push({0, 0, start});

  while (!queue.empty()) {
    const auto [cost, mask, vertex] = queue.top();
    queue.pop();
    if (distance[index_of(mask, vertex)] != cost) {
      continue;
    }
    for (std::size_t edge_id : incident[vertex]) {
      const OracleEdge& edge = edges[edge_id];
      const Vertex next = edge.first == vertex ? edge.second : edge.first;
      const std::size_t next_mask = mask | (std::size_t{1} << edge_id);
      const std::uint64_t weight = static_cast<std::uint64_t>(edge.weight);
      if (cost > inf - weight) {
        continue;
      }
      const std::uint64_t candidate = cost + weight;
      auto& next_distance = distance[index_of(next_mask, next)];
      if (candidate < next_distance) {
        next_distance = candidate;
        queue.push({candidate, next_mask, next});
      }
    }
  }

  const auto answer = distance[index_of(mask_count - 1, start)];
  if (answer == inf) {
    return std::nullopt;
  }
  return answer;
}

void verify_edge_reference(const Graph& graph, const PostmanEdgeReference& ref) {
  REQUIRE(ref.from < graph.vertex_count());
  const auto& neighbors = graph.neighbors(ref.from);
  REQUIRE(ref.adjacency_index < neighbors.size());
  const auto& edge = neighbors[ref.adjacency_index];
  REQUIRE(edge.to == ref.to);
  REQUIRE(edge.weight == ref.weight);
}

void verify_result(const Graph& graph, const ChinesePostmanResult& result) {
  const auto originals = logical_edges(graph);
  if (result.edge_walk.empty()) {
    REQUIRE(originals.empty());
    REQUIRE(result.total_weight == 0);
    if (graph.vertex_count() == 0U) {
      REQUIRE(result.vertices.empty());
    } else {
      REQUIRE(result.vertices == std::vector<Vertex>{0});
    }
    return;
  }

  REQUIRE(result.vertices.size() == result.edge_walk.size() + 1U);
  REQUIRE(result.vertices.front() == result.vertices.back());
  std::uint64_t walk_sum = 0;
  std::set<std::pair<Vertex, std::size_t>> seen_refs;
  for (std::size_t index = 0; index < result.edge_walk.size(); ++index) {
    const auto& step = result.edge_walk[index];
    verify_edge_reference(graph, step.edge);
    const Vertex from = result.vertices[index];
    const Vertex to = result.vertices[index + 1];
    REQUIRE((step.edge.from == from && step.edge.to == to) ||
            (step.edge.from == to && step.edge.to == from));
    walk_sum += static_cast<std::uint64_t>(step.edge.weight);
    seen_refs.insert({step.edge.from, step.edge.adjacency_index});
  }
  REQUIRE(walk_sum == static_cast<std::uint64_t>(result.total_weight));
  REQUIRE(static_cast<std::uint64_t>(result.original_edge_weight) +
              static_cast<std::uint64_t>(result.duplicated_edge_weight) ==
          static_cast<std::uint64_t>(result.total_weight));
  for (const auto& edge : originals) {
    REQUIRE(seen_refs.contains({edge.first, edge.adjacency_index}));
  }

  std::uint64_t augmentation_sum = 0;
  for (const auto& augmentation : result.augmentations) {
    Vertex current = augmentation.first;
    std::uint64_t path_sum = 0;
    for (const auto& ref : augmentation.path) {
      verify_edge_reference(graph, ref);
      REQUIRE(ref.from == current || ref.to == current);
      current = ref.from == current ? ref.to : ref.from;
      path_sum += static_cast<std::uint64_t>(ref.weight);
    }
    REQUIRE(current == augmentation.second);
    REQUIRE(path_sum == static_cast<std::uint64_t>(augmentation.distance));
    augmentation_sum += path_sum;
  }
  REQUIRE(augmentation_sum ==
          static_cast<std::uint64_t>(result.duplicated_edge_weight));
}

TEST_CASE(chinese_postman_trivial_and_adversarial) {
  Graph empty(0, false);
  const auto empty_result = minimum_chinese_postman_tour(empty);
  REQUIRE(empty_result.has_value());
  verify_result(empty, *empty_result);

  Graph edgeless(3, false);
  const auto edgeless_result = minimum_chinese_postman_tour(edgeless);
  REQUIRE(edgeless_result.has_value());
  verify_result(edgeless, *edgeless_result);

  Graph single(2, false);
  single.add_edge(0, 1, 5);
  const auto single_result = minimum_chinese_postman_tour(single);
  REQUIRE(single_result.has_value());
  REQUIRE(single_result->total_weight == 10);
  REQUIRE(single_result->duplicated_edge_weight == 5);
  REQUIRE(single_result->augmentations.size() == 1);
  verify_result(single, *single_result);

  Graph triangle(3, false);
  triangle.add_edge(0, 1, 2);
  triangle.add_edge(1, 2, 3);
  triangle.add_edge(2, 0, 4);
  const auto triangle_result = minimum_chinese_postman_tour(triangle);
  REQUIRE(triangle_result.has_value());
  REQUIRE(triangle_result->total_weight == 9);
  REQUIRE(triangle_result->duplicated_edge_weight == 0);
  REQUIRE(triangle_result->augmentations.empty());
  verify_result(triangle, *triangle_result);

  Graph star(5, false);
  star.add_edge(0, 1, 1);
  star.add_edge(0, 2, 2);
  star.add_edge(0, 3, 3);
  star.add_edge(0, 4, 4);
  const auto star_result = minimum_chinese_postman_tour(star);
  REQUIRE(star_result.has_value());
  REQUIRE(star_result->total_weight == 20);
  REQUIRE(star_result->augmentations.size() == 2);
  verify_result(star, *star_result);

  Graph multi(3, false);
  multi.add_edge(2, 0, 7);
  multi.add_edge(0, 1, 1);
  multi.add_edge(0, 1, 3);
  multi.add_edge(1, 1, 2);
  const auto multi_result = minimum_chinese_postman_tour(multi);
  REQUIRE(multi_result.has_value());
  verify_result(multi, *multi_result);
  const auto repeated = minimum_chinese_postman_tour(multi);
  REQUIRE(repeated == multi_result);

  Graph disconnected(4, false);
  disconnected.add_edge(0, 1, 1);
  disconnected.add_edge(2, 3, 1);
  REQUIRE(!minimum_chinese_postman_tour(disconnected).has_value());

  Graph directed(2, true);
  directed.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(minimum_chinese_postman_tour(directed), std::invalid_argument);

  Graph negative(2, false);
  negative.add_edge(0, 1, -1);
  REQUIRE_THROWS_AS(minimum_chinese_postman_tour(negative), std::invalid_argument);
}

TEST_CASE(chinese_postman_overflow_boundaries) {
  constexpr Weight max = std::numeric_limits<Weight>::max();
  Graph representable(1, false);
  representable.add_edge(0, 0, max);
  const auto result = minimum_chinese_postman_tour(representable);
  REQUIRE(result.has_value());
  REQUIRE(result->total_weight == max);
  verify_result(representable, *result);

  Graph duplicate_overflow(2, false);
  duplicate_overflow.add_edge(0, 1, max);
  REQUIRE_THROWS_AS(minimum_chinese_postman_tour(duplicate_overflow),
                    std::overflow_error);

  Graph base_overflow(2, false);
  base_overflow.add_edge(0, 1, max);
  base_overflow.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(minimum_chinese_postman_tour(base_overflow),
                    std::overflow_error);
}

TEST_CASE(chinese_postman_random_differential) {
  std::mt19937_64 random(0xC11E5E5ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 6);
  std::uniform_int_distribution<int> edge_count_distribution(0, 8);
  std::uniform_int_distribution<int> weight_distribution(0, 9);

  for (int trial = 0; trial < 500; ++trial) {
    const auto vertex_count =
        static_cast<std::size_t>(vertex_count_distribution(random));
    Graph graph(vertex_count, false);
    if (vertex_count != 0U) {
      std::uniform_int_distribution<std::size_t> vertex_distribution(
          0, vertex_count - 1U);
      const int edge_count = edge_count_distribution(random);
      for (int edge = 0; edge < edge_count; ++edge) {
        graph.add_edge(vertex_distribution(random), vertex_distribution(random),
                       static_cast<Weight>(weight_distribution(random)));
      }
    }

    const auto oracle = exact_closed_walk_oracle(graph);
    const auto actual = minimum_chinese_postman_tour(graph);
    REQUIRE(actual.has_value() == oracle.has_value());
    if (!oracle.has_value()) {
      continue;
    }
    REQUIRE(actual->total_weight == static_cast<Weight>(*oracle));
    verify_result(graph, *actual);
    const auto repeated = minimum_chinese_postman_tour(graph);
    REQUIRE(repeated == actual);
  }
}

}  // namespace
