#pragma once

#include "algorithms/graphs/prufer_code.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

std::vector<std::pair<Vertex, Vertex>> canonical_edges(const Graph& graph) {
  std::vector<std::pair<Vertex, Vertex>> edges;
  for (Vertex from = 0U; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        edges.emplace_back(from, edge.to);
      }
    }
  }
  std::sort(edges.begin(), edges.end());
  return edges;
}

bool is_simple_tree_by_bfs(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (graph.directed() || n == 0U) {
    return false;
  }
  std::set<std::pair<Vertex, Vertex>> edges;
  for (Vertex from = 0U; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from == edge.to) {
        return false;
      }
      if (from < edge.to && !edges.emplace(from, edge.to).second) {
        return false;
      }
    }
  }
  if (edges.size() != n - 1U) {
    return false;
  }
  std::vector<bool> seen(n, false);
  std::queue<Vertex> pending;
  seen[0] = true;
  pending.push(0U);
  std::size_t reached = 0U;
  while (!pending.empty()) {
    const Vertex vertex = pending.front();
    pending.pop();
    ++reached;
    for (const auto& edge : graph.neighbors(vertex)) {
      if (!seen[edge.to]) {
        seen[edge.to] = true;
        pending.push(edge.to);
      }
    }
  }
  return reached == n;
}

std::vector<Vertex> naive_smallest_leaf_code(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n <= 2U) {
    return {};
  }
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  std::vector<std::size_t> degree(n, 0U);
  for (Vertex from = 0U; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      adjacent[from][edge.to] = true;
    }
  }
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    for (Vertex other = 0U; other < n; ++other) {
      if (adjacent[vertex][other]) {
        ++degree[vertex];
      }
    }
  }

  std::vector<Vertex> code;
  for (std::size_t step = 0U; step < n - 2U; ++step) {
    Vertex leaf = n;
    for (Vertex candidate = 0U; candidate < n; ++candidate) {
      if (degree[candidate] == 1U) {
        leaf = candidate;
        break;
      }
    }
    REQUIRE(leaf < n);
    Vertex neighbor = n;
    for (Vertex candidate = 0U; candidate < n; ++candidate) {
      if (adjacent[leaf][candidate]) {
        neighbor = candidate;
        break;
      }
    }
    REQUIRE(neighbor < n);
    code.push_back(neighbor);
    adjacent[leaf][neighbor] = false;
    adjacent[neighbor][leaf] = false;
    degree[leaf] = 0U;
    --degree[neighbor];
  }
  return code;
}

std::uint64_t integer_power(std::uint64_t base, std::size_t exponent) {
  std::uint64_t result = 1U;
  for (std::size_t i = 0U; i < exponent; ++i) {
    result *= base;
  }
  return result;
}

}  // namespace

TEST_CASE(prufer_contract_and_known_examples) {
  using algorithms::graphs::prufer_decode;
  using algorithms::graphs::prufer_encode;

  Graph empty(0U, false);
  REQUIRE_THROWS_AS(prufer_encode(empty), std::invalid_argument);
  REQUIRE_THROWS_AS(prufer_decode(0U, std::span<const Vertex>{}), std::invalid_argument);

  Graph singleton(1U, false);
  REQUIRE(prufer_encode(singleton).empty());
  const Graph singleton_roundtrip = prufer_decode(1U, std::span<const Vertex>{});
  REQUIRE(singleton_roundtrip.vertex_count() == 1U);
  REQUIRE(canonical_edges(singleton_roundtrip).empty());

  Graph pair(2U, false);
  pair.add_edge(0U, 1U, -91);
  REQUIRE(prufer_encode(pair).empty());
  const Graph pair_roundtrip = prufer_decode(2U, std::span<const Vertex>{});
  REQUIRE_EQ(canonical_edges(pair_roundtrip),
             (std::vector<std::pair<Vertex, Vertex>>{{0U, 1U}}));

  const std::vector<Vertex> known_code{3U, 3U, 3U, 4U};
  const Graph decoded = prufer_decode(6U, known_code);
  REQUIRE_EQ(prufer_encode(decoded), known_code);
  REQUIRE(is_simple_tree_by_bfs(decoded));

  Graph directed(2U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(prufer_encode(directed), std::invalid_argument);

  Graph self_loop(1U, false);
  self_loop.add_edge(0U, 0U);
  REQUIRE_THROWS_AS(prufer_encode(self_loop), std::invalid_argument);

  Graph parallel(2U, false);
  parallel.add_edge(0U, 1U, 7);
  parallel.add_edge(0U, 1U, -8);
  REQUIRE_THROWS_AS(prufer_encode(parallel), std::invalid_argument);

  Graph cycle(3U, false);
  cycle.add_edge(0U, 1U);
  cycle.add_edge(1U, 2U);
  cycle.add_edge(2U, 0U);
  REQUIRE_THROWS_AS(prufer_encode(cycle), std::invalid_argument);

  Graph disconnected_with_v_minus_one_edges(4U, false);
  disconnected_with_v_minus_one_edges.add_edge(0U, 1U);
  disconnected_with_v_minus_one_edges.add_edge(1U, 2U);
  disconnected_with_v_minus_one_edges.add_edge(2U, 0U);
  REQUIRE_THROWS_AS(prufer_encode(disconnected_with_v_minus_one_edges),
                    std::invalid_argument);

  const std::vector<Vertex> bad_length{0U};
  REQUIRE_THROWS_AS(prufer_decode(2U, bad_length), std::invalid_argument);
  const std::vector<Vertex> bad_vertex{3U};
  REQUIRE_THROWS_AS(prufer_decode(3U, bad_vertex), std::out_of_range);
}

TEST_CASE(prufer_encoding_matches_independent_leaf_scan_on_all_small_trees) {
  using algorithms::graphs::prufer_encode;

  for (std::size_t n = 1U; n <= 5U; ++n) {
    std::vector<std::pair<Vertex, Vertex>> possible_edges;
    for (Vertex first = 0U; first < n; ++first) {
      for (Vertex second = first + 1U; second < n; ++second) {
        possible_edges.emplace_back(first, second);
      }
    }
    const std::uint64_t subset_count = std::uint64_t{1} << possible_edges.size();
    std::size_t tree_count = 0U;
    for (std::uint64_t mask = 0U; mask < subset_count; ++mask) {
      if (static_cast<std::size_t>(std::popcount(mask)) != n - 1U) {
        continue;
      }
      Graph graph(n, false);
      for (std::size_t index = 0U; index < possible_edges.size(); ++index) {
        if ((mask & (std::uint64_t{1} << index)) != 0U) {
          graph.add_edge(possible_edges[index].first,
                         possible_edges[index].second,
                         static_cast<std::int64_t>(index) - 5);
        }
      }
      if (!is_simple_tree_by_bfs(graph)) {
        continue;
      }
      ++tree_count;
      REQUIRE_EQ(prufer_encode(graph), naive_smallest_leaf_code(graph));
    }
    const std::size_t expected_tree_count =
        n == 1U ? 1U : static_cast<std::size_t>(integer_power(n, n - 2U));
    REQUIRE_EQ(tree_count, expected_tree_count);
  }
}

TEST_CASE(prufer_exhaustive_sequence_bijection_and_degree_identity) {
  using algorithms::graphs::prufer_decode;
  using algorithms::graphs::prufer_encode;

  for (std::size_t n = 2U; n <= 7U; ++n) {
    const std::size_t length = n - 2U;
    const std::uint64_t sequence_count = integer_power(n, length);
    std::vector<Vertex> code(length, 0U);
    for (std::uint64_t ordinal = 0U; ordinal < sequence_count; ++ordinal) {
      std::uint64_t value = ordinal;
      for (std::size_t index = 0U; index < length; ++index) {
        code[index] = static_cast<Vertex>(value % n);
        value /= n;
      }

      const Graph graph = prufer_decode(n, code);
      REQUIRE(is_simple_tree_by_bfs(graph));
      REQUIRE_EQ(prufer_encode(graph), code);
      REQUIRE(canonical_edges(graph).size() == n - 1U);

      std::vector<std::size_t> occurrences(n, 0U);
      for (const Vertex vertex : code) {
        ++occurrences[vertex];
      }
      for (Vertex vertex = 0U; vertex < n; ++vertex) {
        REQUIRE_EQ(graph.neighbors(vertex).size(), occurrences[vertex] + 1U);
      }
    }
  }
}

TEST_CASE(prufer_weights_do_not_affect_tree_code) {
  using algorithms::graphs::prufer_encode;

  Graph first(5U, false);
  first.add_edge(0U, 3U, -100);
  first.add_edge(1U, 3U, 22);
  first.add_edge(2U, 3U, 0);
  first.add_edge(3U, 4U, 999);

  Graph second(5U, false);
  second.add_edge(0U, 3U, 17);
  second.add_edge(1U, 3U, -18);
  second.add_edge(2U, 3U, 123456);
  second.add_edge(3U, 4U, -999999);

  REQUIRE_EQ(prufer_encode(first), prufer_encode(second));
  REQUIRE_EQ(prufer_encode(first), (std::vector<Vertex>{3U, 3U, 3U}));
}
