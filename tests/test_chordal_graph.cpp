#include "algorithms/graphs/chordal_graph.hpp"
#include "algorithms/graphs/maximum_clique.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::graphs::ChordalRecognitionResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::recognize_chordal_graph;

namespace {

std::vector<std::vector<std::uint8_t>> simple_matrix(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<std::uint8_t>> adjacent(
      n, std::vector<std::uint8_t>(n, std::uint8_t{0}));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.to == from) continue;
      adjacent[from][edge.to] = 1U;
      adjacent[edge.to][from] = 1U;
    }
  }
  return adjacent;
}

bool is_peo(const std::vector<Vertex>& order,
            const std::vector<std::vector<std::uint8_t>>& adjacent) {
  const std::size_t n = adjacent.size();
  if (order.size() != n) return false;
  std::vector<std::size_t> position(n, n);
  for (std::size_t index = 0; index < n; ++index) {
    if (order[index] >= n || position[order[index]] != n) return false;
    position[order[index]] = index;
  }
  for (std::size_t index = 0; index < n; ++index) {
    const Vertex vertex = order[index];
    std::vector<Vertex> later;
    for (Vertex neighbor = 0; neighbor < n; ++neighbor) {
      if (adjacent[vertex][neighbor] != 0U && position[neighbor] > index) {
        later.push_back(neighbor);
      }
    }
    for (std::size_t first = 0; first < later.size(); ++first) {
      for (std::size_t second = first + 1; second < later.size(); ++second) {
        if (adjacent[later[first]][later[second]] == 0U) return false;
      }
    }
  }
  return true;
}

bool connected_subset(std::uint64_t mask,
                      const std::vector<std::vector<std::uint8_t>>& adjacent) {
  const std::size_t n = adjacent.size();
  Vertex start = n;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (((mask >> vertex) & UINT64_C(1)) != 0U) {
      start = vertex;
      break;
    }
  }
  if (start == n) return false;

  std::uint64_t seen = UINT64_C(1) << start;
  std::vector<Vertex> stack{start};
  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    for (Vertex neighbor = 0; neighbor < n; ++neighbor) {
      if (((mask >> neighbor) & UINT64_C(1)) != 0U &&
          adjacent[vertex][neighbor] != 0U &&
          ((seen >> neighbor) & UINT64_C(1)) == 0U) {
        seen |= UINT64_C(1) << neighbor;
        stack.push_back(neighbor);
      }
    }
  }
  return (seen & mask) == mask;
}

bool exhaustive_chordal_oracle(const Graph& graph) {
  const auto adjacent = simple_matrix(graph);
  const std::size_t n = adjacent.size();
  const std::uint64_t limit = UINT64_C(1) << n;
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    if (std::popcount(mask) < 4) continue;
    bool cycle = true;
    for (Vertex vertex = 0; vertex < n && cycle; ++vertex) {
      if (((mask >> vertex) & UINT64_C(1)) == 0U) continue;
      std::size_t degree = 0;
      for (Vertex neighbor = 0; neighbor < n; ++neighbor) {
        if (((mask >> neighbor) & UINT64_C(1)) != 0U &&
            adjacent[vertex][neighbor] != 0U) {
          ++degree;
        }
      }
      if (degree != 2) cycle = false;
    }
    if (cycle && connected_subset(mask, adjacent)) return false;
  }
  return true;
}

void verify_result(const Graph& graph, const ChordalRecognitionResult& result) {
  const auto adjacent = simple_matrix(graph);
  const std::size_t n = graph.vertex_count();
  REQUIRE_EQ(result.mcs_selection_order.size(), n);
  REQUIRE_EQ(result.candidate_elimination_order.size(), n);

  auto mcs = result.mcs_selection_order;
  auto elimination = result.candidate_elimination_order;
  std::sort(mcs.begin(), mcs.end());
  std::sort(elimination.begin(), elimination.end());
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    REQUIRE_EQ(mcs[vertex], vertex);
    REQUIRE_EQ(elimination[vertex], vertex);
  }

  if (result.chordal) {
    REQUIRE(!result.violation.has_value());
    REQUIRE(result.maximum_clique_size.has_value());
    REQUIRE(is_peo(result.candidate_elimination_order, adjacent));
    const std::size_t exact_clique =
        algorithms::graphs::maximum_clique_bron_kerbosch(graph).vertices.size();
    REQUIRE_EQ(*result.maximum_clique_size, exact_clique);
  } else {
    REQUIRE(result.violation.has_value());
    REQUIRE(!result.maximum_clique_size.has_value());
    REQUIRE(!is_peo(result.candidate_elimination_order, adjacent));
    const auto violation = *result.violation;
    std::vector<std::size_t> position(n);
    for (std::size_t index = 0; index < n; ++index) {
      position[result.candidate_elimination_order[index]] = index;
    }
    REQUIRE(adjacent[violation.vertex][violation.first_later_neighbor] != 0U);
    REQUIRE(adjacent[violation.vertex][violation.second_later_neighbor] != 0U);
    REQUIRE(adjacent[violation.first_later_neighbor][violation.second_later_neighbor] == 0U);
    REQUIRE(position[violation.first_later_neighbor] > position[violation.vertex]);
    REQUIRE(position[violation.second_later_neighbor] > position[violation.vertex]);
  }
}

}  // namespace

TEST_CASE(chordal_recognition_deterministic_shapes) {
  Graph empty(0, false);
  const auto empty_result = recognize_chordal_graph(empty);
  REQUIRE(empty_result.chordal);
  REQUIRE_EQ(*empty_result.maximum_clique_size, std::size_t{0});

  Graph tree(8, false);
  for (Vertex vertex = 1; vertex < 8; ++vertex) tree.add_edge(vertex - 1, vertex);
  const auto tree_result = recognize_chordal_graph(tree);
  REQUIRE(tree_result.chordal);
  verify_result(tree, tree_result);

  Graph clique(5, false);
  for (Vertex first = 0; first < 5; ++first) {
    for (Vertex second = first + 1; second < 5; ++second) clique.add_edge(first, second);
  }
  const auto clique_result = recognize_chordal_graph(clique);
  REQUIRE(clique_result.chordal);
  REQUIRE_EQ(*clique_result.maximum_clique_size, std::size_t{5});
  verify_result(clique, clique_result);

  Graph c4(4, false);
  c4.add_edge(0, 1); c4.add_edge(1, 2); c4.add_edge(2, 3); c4.add_edge(3, 0);
  const auto c4_result = recognize_chordal_graph(c4);
  REQUIRE(!c4_result.chordal);
  verify_result(c4, c4_result);

  Graph chorded_c4 = c4;
  chorded_c4.add_edge(0, 2);
  const auto chorded_result = recognize_chordal_graph(chorded_c4);
  REQUIRE(chorded_result.chordal);
  verify_result(chorded_c4, chorded_result);
}

TEST_CASE(chordal_recognition_multigraph_semantics_and_validation) {
  Graph first(6, false);
  first.add_edge(0, 1, 99);
  first.add_edge(0, 1, -4);
  first.add_edge(1, 1, 123);
  first.add_edge(2, 3);
  first.add_edge(3, 4);
  first.add_edge(2, 4);
  first.add_edge(5, 5, 7);

  Graph second(6, false);
  second.add_edge(0, 1, -1000);
  second.add_edge(2, 3, 4);
  second.add_edge(3, 4, -9);
  second.add_edge(2, 4, 999);
  second.add_edge(5, 5, -77);

  const auto first_result = recognize_chordal_graph(first);
  const auto second_result = recognize_chordal_graph(second);
  REQUIRE_EQ(first_result.chordal, second_result.chordal);
  REQUIRE_EQ(first_result.maximum_clique_size, second_result.maximum_clique_size);
  REQUIRE_EQ(first_result, recognize_chordal_graph(first));

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(recognize_chordal_graph(directed), std::invalid_argument);
}

TEST_CASE(chordal_recognition_randomized_induced_cycle_oracle) {
  std::mt19937_64 rng(UINT64_C(0xC40DDA1));
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % UINT64_C(11));
    Graph graph(n, false);
    for (Vertex first = 0; first < n; ++first) {
      if ((rng() % UINT64_C(8)) == 0U) {
        graph.add_edge(first, first,
                       static_cast<std::int64_t>(rng() % UINT64_C(31)) - 15);
      }
      for (Vertex second = first + 1; second < n; ++second) {
        const std::size_t copies = static_cast<std::size_t>(rng() % UINT64_C(4));
        for (std::size_t copy = 0; copy < copies; ++copy) {
          if ((rng() % UINT64_C(3)) == 0U) {
            graph.add_edge(first, second,
                           static_cast<std::int64_t>(rng() % UINT64_C(101)) - 50);
          }
        }
      }
    }

    const bool expected = exhaustive_chordal_oracle(graph);
    const auto result = recognize_chordal_graph(graph);
    REQUIRE_EQ(result.chordal, expected);
    verify_result(graph, result);
  }
}

TEST_CASE(chordal_recognition_peo_violation_is_replayable) {
  Graph cycle(5, false);
  for (Vertex vertex = 0; vertex < 5; ++vertex) cycle.add_edge(vertex, (vertex + 1) % 5);
  const auto result = recognize_chordal_graph(cycle);
  REQUIRE(!result.chordal);
  REQUIRE(result.violation.has_value());
  verify_result(cycle, result);
}
