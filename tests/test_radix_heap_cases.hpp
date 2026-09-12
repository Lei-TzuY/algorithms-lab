#pragma once

#include "algorithms/data_structures/radix_heap.hpp"
#include "algorithms/graphs/radix_dijkstra.hpp"
#include "algorithms/graphs/shortest_paths.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace radix_recovery_test_detail {

[[nodiscard]] inline std::vector<std::optional<algorithms::graphs::Weight>>
bellman_oracle(const algorithms::graphs::Graph& graph,
               algorithms::graphs::Vertex source) {
  using algorithms::graphs::Edge;
  using algorithms::graphs::Vertex;
  using algorithms::graphs::Weight;

  graph.validate_vertex(source);
  for (const auto& edges : graph.adjacency()) {
    for (const Edge& edge : edges) {
      if (edge.weight < 0) {
        throw std::invalid_argument("oracle requires non-negative weights");
      }
    }
  }

  std::vector<std::optional<Weight>> distance(graph.vertex_count());
  distance[source] = Weight{0};
  for (std::size_t pass = 1U; pass < graph.vertex_count(); ++pass) {
    bool changed = false;
    for (Vertex from = 0; from < graph.vertex_count(); ++from) {
      if (!distance[from].has_value()) {
        continue;
      }
      for (const Edge& edge : graph.neighbors(from)) {
        if (edge.weight > 0 &&
            *distance[from] >
                std::numeric_limits<Weight>::max() - edge.weight) {
          throw std::overflow_error("oracle distance overflow");
        }
        const Weight candidate = *distance[from] + edge.weight;
        if (!distance[edge.to].has_value() || candidate < *distance[edge.to]) {
          distance[edge.to] = candidate;
          changed = true;
        }
      }
    }
    if (!changed) {
      break;
    }
  }
  return distance;
}

inline void replay_parents(
    const algorithms::graphs::Graph& graph, algorithms::graphs::Vertex source,
    const algorithms::graphs::RadixShortestPathResult& result) {
  using algorithms::graphs::Vertex;
  using algorithms::graphs::Weight;

  REQUIRE_EQ(result.distance.size(), graph.vertex_count());
  REQUIRE_EQ(result.parent.size(), graph.vertex_count());
  REQUIRE(result.distance[source].has_value());
  REQUIRE_EQ(*result.distance[source], Weight{0});
  REQUIRE(!result.parent[source].has_value());

  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (vertex == source || !result.distance[vertex].has_value()) {
      continue;
    }
    REQUIRE(result.parent[vertex].has_value());
    const Vertex parent = *result.parent[vertex];
    REQUIRE(result.distance[parent].has_value());
    bool witnessed = false;
    for (const auto& edge : graph.neighbors(parent)) {
      if (edge.to != vertex || edge.weight < 0) {
        continue;
      }
      if (edge.weight <= 0 ||
          *result.distance[parent] <=
              std::numeric_limits<Weight>::max() - edge.weight) {
        if (*result.distance[parent] + edge.weight == *result.distance[vertex]) {
          witnessed = true;
          break;
        }
      }
    }
    REQUIRE(witnessed);
  }
}

}  // namespace radix_recovery_test_detail

TEST_CASE(radix_heap_monotone_contract_and_full_width_keys) {
  algorithms::data_structures::RadixHeap<int> heap;
  REQUIRE(heap.empty());
  REQUIRE(heap.valid_structure());
  REQUIRE_THROWS_AS(heap.pop(), std::out_of_range);

  heap.push(0U, 10);
  heap.push(7U, 70);
  heap.push(3U, 30);
  heap.push(3U, 31);
  REQUIRE(heap.valid_structure());
  REQUIRE_EQ(heap.size(), 4U);

  const auto zero = heap.pop();
  REQUIRE_EQ(zero.key, 0U);
  const auto first_three = heap.pop();
  const auto second_three = heap.pop();
  REQUIRE_EQ(first_three.key, 3U);
  REQUIRE_EQ(second_three.key, 3U);
  REQUIRE(heap.valid_structure());
  REQUIRE_THROWS_AS(heap.push(2U, 20), std::invalid_argument);

  heap.push(std::numeric_limits<std::uint64_t>::max(), 99);
  REQUIRE_EQ(heap.pop().key, 7U);
  REQUIRE_EQ(heap.pop().key, std::numeric_limits<std::uint64_t>::max());
  REQUIRE(heap.empty());
  REQUIRE(heap.redistribution_count() > 0U);
}

TEST_CASE(radix_heap_randomized_differential_against_multiset) {
  algorithms::data_structures::RadixHeap<std::uint64_t> heap;
  std::multiset<std::pair<std::uint64_t, std::uint64_t>> oracle;
  std::mt19937_64 rng(0x5241444958484541ULL);
  std::uint64_t next_id = 0U;

  for (std::size_t step = 0; step < 30000U; ++step) {
    const bool push = oracle.empty() || (rng() & 3U) != 0U;
    if (push) {
      const std::uint64_t base = heap.last_popped_key();
      const std::uint64_t delta = rng() % 100000U;
      const std::uint64_t key =
          base > std::numeric_limits<std::uint64_t>::max() - delta
              ? std::numeric_limits<std::uint64_t>::max()
              : base + delta;
      heap.push(key, next_id);
      oracle.insert({key, next_id});
      ++next_id;
    } else {
      const auto entry = heap.pop();
      REQUIRE_EQ(entry.key, oracle.begin()->first);
      const auto iterator = oracle.find({entry.key, entry.value});
      REQUIRE(iterator != oracle.end());
      oracle.erase(iterator);
    }
    REQUIRE(heap.valid_structure());
    REQUIRE_EQ(heap.size(), oracle.size());
  }
}

TEST_CASE(radix_heap_dijkstra_contract_and_adversarial_shapes) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::Weight;

  Graph graph(6U, true);
  graph.add_edge(0U, 1U, 5);
  graph.add_edge(0U, 1U, 2);  // parallel edge
  graph.add_edge(1U, 2U, 0);
  graph.add_edge(2U, 2U, 0);  // self-loop
  graph.add_edge(2U, 3U, 7);
  graph.add_edge(0U, 3U, 20);
  graph.add_edge(3U, 4U, 1);

  const auto result = algorithms::graphs::radix_heap_dijkstra(graph, 0U);
  const auto oracle = radix_recovery_test_detail::bellman_oracle(graph, 0U);
  REQUIRE_EQ(result.distance, oracle);
  REQUIRE_EQ(result.distance, algorithms::graphs::dijkstra(graph, 0U).distance);
  REQUIRE(!result.distance[5U].has_value());
  radix_recovery_test_detail::replay_parents(graph, 0U, result);

  Graph negative(2U, true);
  negative.add_edge(1U, 1U, -1);
  REQUIRE_THROWS_AS(algorithms::graphs::radix_heap_dijkstra(negative, 0U),
                    std::invalid_argument);

  Graph overflow(3U, true);
  overflow.add_edge(0U, 1U, std::numeric_limits<Weight>::max());
  overflow.add_edge(1U, 2U, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::radix_heap_dijkstra(overflow, 0U),
                    std::overflow_error);
}

TEST_CASE(radix_heap_dijkstra_randomized_three_way_differential) {
  using algorithms::graphs::Graph;
  std::mt19937_64 rng(0xD1A6A0ADULL);

  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const std::size_t vertex_count = 1U + static_cast<std::size_t>(rng() % 25U);
    Graph graph(vertex_count, (rng() & 1U) != 0U);
    const std::size_t edge_count = static_cast<std::size_t>(rng() % 120U);
    for (std::size_t edge = 0; edge < edge_count; ++edge) {
      graph.add_edge(static_cast<std::size_t>(rng() % vertex_count),
                     static_cast<std::size_t>(rng() % vertex_count),
                     static_cast<std::int64_t>(rng() % 1000U));
    }

    const std::size_t source = static_cast<std::size_t>(rng() % vertex_count);
    const auto radix = algorithms::graphs::radix_heap_dijkstra(graph, source);
    const auto bellman = radix_recovery_test_detail::bellman_oracle(graph, source);
    const auto binary = algorithms::graphs::dijkstra(graph, source);
    REQUIRE_EQ(radix.distance, bellman);
    REQUIRE_EQ(radix.distance, binary.distance);
    radix_recovery_test_detail::replay_parents(graph, source, radix);
  }
}
