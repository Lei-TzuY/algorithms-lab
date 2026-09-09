#include "algorithms/graphs/dominance_frontier.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::graphs::DominanceFrontierIndex;
using algorithms::graphs::Edge;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

struct FrontierOracle {
  std::vector<unsigned char> reachable;
  std::vector<std::vector<unsigned char>> dominators;
  std::vector<std::vector<Vertex>> frontiers;
};

FrontierOracle build_frontier_oracle(const Graph& graph, Vertex start) {
  const std::size_t vertex_count = graph.vertex_count();
  FrontierOracle oracle;
  oracle.reachable.assign(vertex_count, 0U);
  oracle.dominators.assign(
      vertex_count, std::vector<unsigned char>(vertex_count, 0U));
  oracle.frontiers.resize(vertex_count);

  std::vector<Vertex> reachable_order{start};
  oracle.reachable[start] = 1U;
  for (std::size_t cursor = 0U; cursor < reachable_order.size(); ++cursor) {
    const Vertex source = reachable_order[cursor];
    for (const Edge& edge : graph.neighbors(source)) {
      if (oracle.reachable[edge.to] == 0U) {
        oracle.reachable[edge.to] = 1U;
        reachable_order.push_back(edge.to);
      }
    }
  }

  std::vector<std::vector<Vertex>> predecessors(vertex_count);
  for (Vertex source = 0U; source < vertex_count; ++source) {
    if (oracle.reachable[source] == 0U) {
      continue;
    }
    for (const Edge& edge : graph.neighbors(source)) {
      if (oracle.reachable[edge.to] != 0U) {
        predecessors[edge.to].push_back(source);
      }
    }
  }

  for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
    if (oracle.reachable[vertex] == 0U) {
      continue;
    }
    if (vertex == start) {
      oracle.dominators[vertex][start] = 1U;
    } else {
      for (Vertex candidate = 0U; candidate < vertex_count; ++candidate) {
        if (oracle.reachable[candidate] != 0U) {
          oracle.dominators[vertex][candidate] = 1U;
        }
      }
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
      if (oracle.reachable[vertex] == 0U || vertex == start) {
        continue;
      }

      std::vector<unsigned char> next(vertex_count, 0U);
      bool first_predecessor = true;
      for (const Vertex predecessor : predecessors[vertex]) {
        if (first_predecessor) {
          next = oracle.dominators[predecessor];
          first_predecessor = false;
        } else {
          for (Vertex candidate = 0U; candidate < vertex_count; ++candidate) {
            next[candidate] = static_cast<unsigned char>(
                next[candidate] != 0U &&
                oracle.dominators[predecessor][candidate] != 0U);
          }
        }
      }
      next[vertex] = 1U;
      if (next != oracle.dominators[vertex]) {
        oracle.dominators[vertex] = std::move(next);
        changed = true;
      }
    }
  }

  for (Vertex dominator = 0U; dominator < vertex_count; ++dominator) {
    if (oracle.reachable[dominator] == 0U) {
      continue;
    }
    for (Vertex block = 0U; block < vertex_count; ++block) {
      if (oracle.reachable[block] == 0U) {
        continue;
      }
      const bool strictly_dominates =
          dominator != block && oracle.dominators[block][dominator] != 0U;
      if (strictly_dominates) {
        continue;
      }

      bool dominates_predecessor = false;
      for (const Vertex predecessor : predecessors[block]) {
        if (oracle.dominators[predecessor][dominator] != 0U) {
          dominates_predecessor = true;
          break;
        }
      }
      if (dominates_predecessor) {
        oracle.frontiers[dominator].push_back(block);
      }
    }
  }

  return oracle;
}

std::vector<Vertex> oracle_iterated_frontier(
    const FrontierOracle& oracle, const std::vector<Vertex>& definitions) {
  const std::size_t vertex_count = oracle.reachable.size();
  std::vector<unsigned char> is_definition(vertex_count, 0U);
  std::vector<unsigned char> in_result(vertex_count, 0U);
  std::vector<unsigned char> processed(vertex_count, 0U);

  for (const Vertex definition : definitions) {
    if (definition >= vertex_count || oracle.reachable[definition] == 0U) {
      throw std::invalid_argument("oracle definition outside reachable domain");
    }
    is_definition[definition] = 1U;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (Vertex source = 0U; source < vertex_count; ++source) {
      if (is_definition[source] == 0U && in_result[source] == 0U) {
        continue;
      }
      if (processed[source] != 0U) {
        continue;
      }
      processed[source] = 1U;
      for (const Vertex block : oracle.frontiers[source]) {
        if (in_result[block] == 0U) {
          in_result[block] = 1U;
          changed = true;
        }
      }
    }
  }

  std::vector<Vertex> result;
  for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
    if (in_result[vertex] != 0U) {
      result.push_back(vertex);
    }
  }
  return result;
}

TEST_CASE(dominance_frontier_diamond_and_idf) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  graph.add_edge(1U, 3U);
  graph.add_edge(2U, 3U);

  const DominanceFrontierIndex index(graph, 0U);
  REQUIRE_EQ(index.vertex_count(), 4U);
  REQUIRE_EQ(index.start(), 0U);
  REQUIRE_EQ(index.frontier(0U), std::vector<Vertex>{});
  REQUIRE_EQ(index.frontier(1U), std::vector<Vertex>{3U});
  REQUIRE_EQ(index.frontier(2U), std::vector<Vertex>{3U});
  REQUIRE_EQ(index.frontier(3U), std::vector<Vertex>{});
  REQUIRE_EQ(index.iterated_frontier({1U, 2U}), std::vector<Vertex>{3U});
  REQUIRE_EQ(index.iterated_frontier({}), std::vector<Vertex>{});
}

TEST_CASE(dominance_frontier_loop_self_frontier_and_closure) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);
  graph.add_edge(2U, 1U);
  graph.add_edge(1U, 3U);

  const DominanceFrontierIndex index(graph, 0U);
  REQUIRE_EQ(index.frontier(1U), std::vector<Vertex>{1U});
  REQUIRE_EQ(index.frontier(2U), std::vector<Vertex>{1U});
  REQUIRE_EQ(index.iterated_frontier({2U}), std::vector<Vertex>{1U});
  REQUIRE_EQ(index.iterated_frontier({1U}), std::vector<Vertex>{1U});

  Graph singleton(1U, true);
  singleton.add_edge(0U, 0U);
  const DominanceFrontierIndex self(singleton, 0U);
  REQUIRE_EQ(self.frontier(0U), std::vector<Vertex>{0U});
  REQUIRE_EQ(self.iterated_frontier({0U}), std::vector<Vertex>{0U});
}

TEST_CASE(dominance_frontier_multigraph_unreachable_and_validation) {
  Graph graph(5U, true);
  graph.add_edge(0U, 1U, 7);
  graph.add_edge(0U, 1U, -9);
  graph.add_edge(0U, 2U, 3);
  graph.add_edge(1U, 3U, 11);
  graph.add_edge(1U, 3U, -4);
  graph.add_edge(2U, 3U, 99);
  graph.add_edge(3U, 3U, -8);
  graph.add_edge(4U, 3U, 123);  // unreachable predecessor is outside the domain

  const DominanceFrontierIndex first(graph, 0U);
  const DominanceFrontierIndex second(graph, 0U);
  REQUIRE(first.reachable(0U));
  REQUIRE(!first.reachable(4U));
  REQUIRE_EQ(first.frontier(1U), std::vector<Vertex>{3U});
  REQUIRE_EQ(first.frontier(2U), std::vector<Vertex>{3U});
  REQUIRE_EQ(first.frontier(4U), std::vector<Vertex>{});
  REQUIRE_EQ(first.frontiers(), second.frontiers());
  REQUIRE_EQ(first.iterated_frontier({1U, 1U, 2U}),
             std::vector<Vertex>{3U});

  REQUIRE_THROWS_AS(first.iterated_frontier({4U}), std::invalid_argument);
  REQUIRE_THROWS_AS(first.frontier(5U), std::out_of_range);
  REQUIRE_THROWS_AS(first.iterated_frontier({5U}), std::out_of_range);

  Graph undirected(2U, false);
  undirected.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(DominanceFrontierIndex(undirected, 0U),
                    std::invalid_argument);

  Graph empty(0U, true);
  REQUIRE_THROWS_AS(DominanceFrontierIndex(empty, 0U), std::out_of_range);
}

TEST_CASE(dominance_frontier_randomized_definition_differential) {
  std::mt19937_64 random(0xD0F14AULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(random() % 12U);
    Graph graph(vertex_count, true);

    for (Vertex source = 0U; source < vertex_count; ++source) {
      for (Vertex target = 0U; target < vertex_count; ++target) {
        if ((random() % 100U) < 24U) {
          const std::int64_t weight =
              static_cast<std::int64_t>(random() % 101U) - 50;
          graph.add_edge(source, target, weight);
          if ((random() % 7U) == 0U) {
            graph.add_edge(source, target, -weight);
          }
        }
      }
    }

    const Vertex start = static_cast<Vertex>(random() % vertex_count);
    const DominanceFrontierIndex index(graph, start);
    const FrontierOracle oracle = build_frontier_oracle(graph, start);

    for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
      REQUIRE_EQ(index.reachable(vertex), oracle.reachable[vertex] != 0U);
      REQUIRE_EQ(index.frontier(vertex), oracle.frontiers[vertex]);
    }

    std::vector<Vertex> reachable;
    for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
      if (oracle.reachable[vertex] != 0U) {
        reachable.push_back(vertex);
      }
    }

    for (std::size_t query = 0U; query < 8U; ++query) {
      std::vector<Vertex> definitions;
      const std::size_t definition_count =
          static_cast<std::size_t>(random() % (reachable.size() + 1U));
      for (std::size_t index_in_set = 0U;
           index_in_set < definition_count; ++index_in_set) {
        definitions.push_back(
            reachable[static_cast<std::size_t>(random() % reachable.size())]);
      }
      REQUIRE_EQ(index.iterated_frontier(definitions),
                 oracle_iterated_frontier(oracle, definitions));
    }
  }
}

}  // namespace
