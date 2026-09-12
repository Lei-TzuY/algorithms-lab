#include "algorithms/combinatorial/dilworth_decomposition.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "test_framework.hpp"

using algorithms::combinatorial::DilworthDecompositionResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

namespace {

std::vector<std::vector<bool>> floyd_reachability(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> reachable(n, std::vector<bool>(n, false));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      reachable[from][edge.to] = true;
    }
  }
  for (std::size_t k = 0; k < n; ++k) {
    for (std::size_t i = 0; i < n; ++i) {
      if (!reachable[i][k]) {
        continue;
      }
      for (std::size_t j = 0; j < n; ++j) {
        reachable[i][j] = reachable[i][j] || reachable[k][j];
      }
    }
  }
  return reachable;
}

bool is_antichain_mask(std::size_t mask,
                       const std::vector<std::vector<bool>>& reachable) {
  for (std::size_t first = 0; first < reachable.size(); ++first) {
    if ((mask & (std::size_t{1} << first)) == 0) {
      continue;
    }
    for (std::size_t second = first + 1; second < reachable.size(); ++second) {
      if ((mask & (std::size_t{1} << second)) != 0 &&
          (reachable[first][second] || reachable[second][first])) {
        return false;
      }
    }
  }
  return true;
}

std::size_t exhaustive_width(
    const std::vector<std::vector<bool>>& reachable) {
  const std::size_t n = reachable.size();
  std::size_t best = 0;
  const std::size_t limit = std::size_t{1} << n;
  for (std::size_t mask = 0; mask < limit; ++mask) {
    if (is_antichain_mask(mask, reachable)) {
      best = std::max(best, static_cast<std::size_t>(std::popcount(mask)));
    }
  }
  return best;
}

void validate_result(const Graph& graph,
                     const DilworthDecompositionResult& result) {
  const auto reachable = floyd_reachability(graph);
  const std::size_t exact_width = exhaustive_width(reachable);
  REQUIRE_EQ(result.width, exact_width);
  REQUIRE_EQ(result.matching_cardinality + result.width,
             graph.vertex_count());
  REQUIRE_EQ(result.minimum_chain_decomposition.size(), result.width);
  REQUIRE_EQ(result.maximum_antichain.size(), result.width);

  std::vector<bool> seen(graph.vertex_count(), false);
  for (const auto& chain : result.minimum_chain_decomposition) {
    REQUIRE(!chain.empty());
    for (std::size_t index = 0; index < chain.size(); ++index) {
      const std::size_t vertex = chain[index];
      REQUIRE(vertex < graph.vertex_count());
      REQUIRE(!seen[vertex]);
      seen[vertex] = true;
      if (index > 0) {
        REQUIRE(reachable[chain[index - 1]][vertex]);
      }
    }
  }
  REQUIRE(std::all_of(seen.begin(), seen.end(),
                      [](bool value) { return value; }));

  for (std::size_t index = 1; index < result.maximum_antichain.size(); ++index) {
    REQUIRE(result.maximum_antichain[index - 1] <
            result.maximum_antichain[index]);
  }
  for (std::size_t first = 0; first < result.maximum_antichain.size(); ++first) {
    for (std::size_t second = first + 1;
         second < result.maximum_antichain.size(); ++second) {
      const std::size_t a = result.maximum_antichain[first];
      const std::size_t b = result.maximum_antichain[second];
      REQUIRE(!reachable[a][b]);
      REQUIRE(!reachable[b][a]);
    }
  }
}

}  // namespace

TEST_CASE(dilworth_deterministic_contracts_and_transitive_closure) {
  {
    Graph graph(0, true);
    const auto result = algorithms::combinatorial::dilworth_decomposition(graph);
    REQUIRE_EQ(result.width, std::size_t{0});
    REQUIRE(result.maximum_antichain.empty());
    REQUIRE(result.minimum_chain_decomposition.empty());
  }
  {
    Graph graph(4, true);
    const auto result = algorithms::combinatorial::dilworth_decomposition(graph);
    validate_result(graph, result);
    REQUIRE_EQ(result.width, std::size_t{4});
  }
  {
    Graph graph(5, true);
    for (std::size_t vertex = 0; vertex + 1 < 5; ++vertex) {
      graph.add_edge(vertex, vertex + 1);
    }
    const auto result = algorithms::combinatorial::dilworth_decomposition(graph);
    validate_result(graph, result);
    REQUIRE_EQ(result.width, std::size_t{1});
  }
  {
    Graph graph(4, true);
    graph.add_edge(0, 1);
    graph.add_edge(0, 2);
    graph.add_edge(1, 3);
    graph.add_edge(2, 3);
    const auto result = algorithms::combinatorial::dilworth_decomposition(graph);
    validate_result(graph, result);
    REQUIRE_EQ(result.width, std::size_t{2});
  }

  // Direct-edge path-cover reasoning would return three chains here. Dilworth
  // must use the transitive comparability relation and returns width two.
  {
    Graph graph(5, true);
    graph.add_edge(0, 2);
    graph.add_edge(1, 2);
    graph.add_edge(2, 3);
    graph.add_edge(2, 4);
    const auto result = algorithms::combinatorial::dilworth_decomposition(graph);
    validate_result(graph, result);
    REQUIRE_EQ(result.width, std::size_t{2});
  }

  {
    Graph graph(3, true);
    graph.add_edge(0, 1, 99);
    graph.add_edge(0, 1, -5);
    graph.add_edge(1, 2, 7);
    const auto first = algorithms::combinatorial::dilworth_decomposition(graph);
    const auto second = algorithms::combinatorial::dilworth_decomposition(graph);
    validate_result(graph, first);
    REQUIRE_EQ(first.width, std::size_t{1});
    REQUIRE_EQ(first.minimum_chain_decomposition,
               second.minimum_chain_decomposition);
    REQUIRE_EQ(first.maximum_antichain, second.maximum_antichain);
  }

  Graph undirected(2, false);
  REQUIRE_THROWS_AS(algorithms::combinatorial::dilworth_decomposition(undirected),
                    std::invalid_argument);

  Graph cyclic(2, true);
  cyclic.add_edge(0, 1);
  cyclic.add_edge(1, 0);
  REQUIRE_THROWS_AS(algorithms::combinatorial::dilworth_decomposition(cyclic),
                    std::invalid_argument);
}

TEST_CASE(dilworth_randomized_differential_against_exhaustive_antichains) {
  std::mt19937_64 rng(0xD11A0A7ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 9);
  std::uniform_int_distribution<int> weight_distribution(-20, 20);
  std::uniform_int_distribution<int> percent(0, 99);

  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t n =
        static_cast<std::size_t>(vertex_count_distribution(rng));
    Graph graph(n, true);
    std::vector<std::size_t> order(n);
    for (std::size_t index = 0; index < n; ++index) {
      order[index] = index;
    }
    std::shuffle(order.begin(), order.end(), rng);

    for (std::size_t first = 0; first < n; ++first) {
      for (std::size_t second = first + 1; second < n; ++second) {
        if (percent(rng) >= 28) {
          continue;
        }
        graph.add_edge(order[first], order[second], weight_distribution(rng));
        if (percent(rng) < 15) {
          graph.add_edge(order[first], order[second], weight_distribution(rng));
        }
      }
    }

    const auto first = algorithms::combinatorial::dilworth_decomposition(graph);
    const auto second = algorithms::combinatorial::dilworth_decomposition(graph);
    validate_result(graph, first);
    REQUIRE_EQ(first.width, second.width);
    REQUIRE_EQ(first.matching_cardinality, second.matching_cardinality);
    REQUIRE_EQ(first.minimum_chain_decomposition,
               second.minimum_chain_decomposition);
    REQUIRE_EQ(first.maximum_antichain, second.maximum_antichain);
  }
}
