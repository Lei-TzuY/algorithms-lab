#pragma once

#include "algorithms/graphs/rake_compress_tree_contraction.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rake_compress_test_detail {
using algorithms::graphs::Graph;
using algorithms::graphs::RakeCompressTreeContraction;
using algorithms::graphs::TreeContractionKind;
using algorithms::graphs::TreeContractionStep;
using algorithms::graphs::Vertex;

struct ReplayState {
  std::vector<std::set<Vertex>> adjacency;
  std::vector<bool> active;
  std::size_t active_count = 0;
};

inline ReplayState independent_tree_state(const Graph& graph) {
  ReplayState state;
  state.adjacency.resize(graph.vertex_count());
  state.active.assign(graph.vertex_count(), true);
  state.active_count = graph.vertex_count();
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        state.adjacency[from].insert(edge.to);
        state.adjacency[edge.to].insert(from);
      }
    }
  }
  return state;
}

inline std::pair<Vertex, Vertex> neighbors2(const std::set<Vertex>& neighbors) {
  auto iterator = neighbors.begin();
  const Vertex first = *iterator;
  ++iterator;
  return {first, *iterator};
}

inline void replay_independently(const Graph& graph,
                                 const RakeCompressTreeContraction& result) {
  ReplayState state = independent_tree_state(graph);
  for (std::size_t round_index = 0; round_index < result.rounds.size(); ++round_index) {
    const auto& round = result.rounds[round_index];
    REQUIRE_EQ(round.active_before, state.active_count);

    std::vector<bool> selected(graph.vertex_count(), false);
    for (const TreeContractionStep& step : round.compressions) {
      REQUIRE(step.kind == TreeContractionKind::compress);
      REQUIRE(step.vertex != result.root);
      REQUIRE(state.active[step.vertex]);
      REQUIRE_EQ(state.adjacency[step.vertex].size(), std::size_t{2});
      const auto [first, second] = neighbors2(state.adjacency[step.vertex]);
      REQUIRE_EQ(step.first_neighbor, first);
      REQUIRE(step.second_neighbor.has_value());
      REQUIRE_EQ(*step.second_neighbor, second);
      for (const Vertex neighbor : state.adjacency[step.vertex]) {
        REQUIRE(!selected[neighbor]);
      }
      selected[step.vertex] = true;
    }

    // Maximality is checked independently: every unselected eligible degree-two
    // vertex must touch one selected compression vertex.
    for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
      if (!state.active[vertex] || vertex == result.root ||
          state.adjacency[vertex].size() != 2 || selected[vertex]) {
        continue;
      }
      bool dominated = false;
      for (const Vertex neighbor : state.adjacency[vertex]) {
        dominated = dominated || selected[neighbor];
      }
      REQUIRE(dominated);
    }

    for (const TreeContractionStep& step : round.compressions) {
      const auto [first, second] = neighbors2(state.adjacency[step.vertex]);
      state.adjacency[first].erase(step.vertex);
      state.adjacency[second].erase(step.vertex);
      state.adjacency[step.vertex].clear();
      state.adjacency[first].insert(second);
      state.adjacency[second].insert(first);
      state.active[step.vertex] = false;
      --state.active_count;
    }

    std::vector<Vertex> expected_rakes;
    for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
      if (state.active[vertex] && vertex != result.root &&
          state.adjacency[vertex].size() == 1) {
        expected_rakes.push_back(vertex);
      }
    }
    REQUIRE_EQ(round.rakes.size(), expected_rakes.size());
    for (std::size_t index = 0; index < expected_rakes.size(); ++index) {
      const auto& step = round.rakes[index];
      const Vertex vertex = expected_rakes[index];
      REQUIRE(step.kind == TreeContractionKind::rake);
      REQUIRE_EQ(step.vertex, vertex);
      REQUIRE(!step.second_neighbor.has_value());
      REQUIRE_EQ(step.first_neighbor, *state.adjacency[vertex].begin());
    }
    for (const Vertex vertex : expected_rakes) {
      const Vertex neighbor = *state.adjacency[vertex].begin();
      state.adjacency[neighbor].erase(vertex);
      state.adjacency[vertex].clear();
      state.active[vertex] = false;
      --state.active_count;
    }

    REQUIRE_EQ(round.active_after, state.active_count);
    REQUIRE(round.active_after < round.active_before);
    if (round.active_after >= 2 && round.active_before > 2) {
      REQUIRE(3 * (round.active_after - 2) <= 2 * (round.active_before - 2));
    }
  }
  REQUIRE_EQ(state.active_count, std::size_t{1});
  REQUIRE(state.active[result.root]);
  REQUIRE(state.adjacency[result.root].empty());
}

inline Graph shuffled_tree(std::size_t n, std::mt19937_64& rng,
                           std::vector<std::pair<Vertex, Vertex>>& edges) {
  edges.clear();
  for (Vertex vertex = 1; vertex < n; ++vertex) {
    edges.emplace_back(static_cast<Vertex>(rng() % vertex), vertex);
  }
  std::shuffle(edges.begin(), edges.end(), rng);
  Graph graph(n, false);
  for (const auto& [first, second] : edges) {
    const std::uint64_t raw = rng();
    graph.add_edge(first, second, static_cast<std::int64_t>(raw));
  }
  return graph;
}
}  // namespace rake_compress_test_detail

TEST_CASE(rake_compress_rejects_non_trees) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::rake_compress_tree_contraction;
  REQUIRE_THROWS_AS(rake_compress_tree_contraction(Graph(0, false), 0), std::invalid_argument);

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(rake_compress_tree_contraction(directed, 0), std::invalid_argument);

  Graph self_loop(1, false);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(rake_compress_tree_contraction(self_loop, 0), std::invalid_argument);

  Graph parallel(2, false);
  parallel.add_edge(0, 1);
  parallel.add_edge(0, 1);
  REQUIRE_THROWS_AS(rake_compress_tree_contraction(parallel, 0), std::invalid_argument);

  Graph cycle(3, false);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 2);
  cycle.add_edge(2, 0);
  REQUIRE_THROWS_AS(rake_compress_tree_contraction(cycle, 0), std::invalid_argument);

  Graph singleton(1, false);
  REQUIRE_THROWS_AS(rake_compress_tree_contraction(singleton, 1), std::out_of_range);
}

TEST_CASE(rake_compress_known_shapes_and_metadata) {
  using namespace algorithms::graphs;
  using namespace rake_compress_test_detail;

  Graph singleton(1, false);
  const auto one = rake_compress_tree_contraction(singleton, 0);
  REQUIRE(one.rounds.empty());
  REQUIRE(valid_rake_compress_tree_contraction(singleton, one));
  REQUIRE(!one.removal_round[0].has_value());

  Graph star(20, false);
  for (Vertex vertex = 1; vertex < 20; ++vertex) {
    star.add_edge(0, vertex, -static_cast<std::int64_t>(vertex));
  }
  const auto star_result = rake_compress_tree_contraction(star, 0);
  REQUIRE_EQ(star_result.rounds.size(), std::size_t{1});
  REQUIRE(star_result.rounds[0].compressions.empty());
  REQUIRE_EQ(star_result.rounds[0].rakes.size(), std::size_t{19});
  REQUIRE(valid_rake_compress_tree_contraction(star, star_result));
  replay_independently(star, star_result);

  Graph path(17, false);
  for (Vertex vertex = 1; vertex < 17; ++vertex) {
    path.add_edge(vertex - 1, vertex, static_cast<std::int64_t>(vertex));
  }
  const auto path_result = rake_compress_tree_contraction(path, 0);
  REQUIRE(valid_rake_compress_tree_contraction(path, path_result));
  REQUIRE(std::any_of(path_result.rounds.begin(), path_result.rounds.end(),
                      [](const auto& round) { return !round.compressions.empty(); }));
  replay_independently(path, path_result);
}

TEST_CASE(rake_compress_long_path_has_logarithmic_round_shrink) {
  using namespace algorithms::graphs;
  using namespace rake_compress_test_detail;
  constexpr std::size_t n = 2048;
  Graph graph(n, false);
  for (Vertex vertex = 1; vertex < n; ++vertex) {
    graph.add_edge(vertex - 1, vertex);
  }
  const auto result = rake_compress_tree_contraction(graph, n / 2);
  REQUIRE(valid_rake_compress_tree_contraction(graph, result));
  replay_independently(graph, result);
  REQUIRE(result.rounds.size() < 32);
}

TEST_CASE(rake_compress_randomized_schedule_replay) {
  using namespace algorithms::graphs;
  using namespace rake_compress_test_detail;
  std::mt19937_64 rng(0x52414B45434F4D50ULL);
  std::vector<std::pair<Vertex, Vertex>> edges;
  for (int trial = 0; trial < 400; ++trial) {
    const std::size_t n = 1 + static_cast<std::size_t>(rng() % 96U);
    Graph graph = shuffled_tree(n, rng, edges);
    const Vertex root = static_cast<Vertex>(rng() % n);
    const auto first = rake_compress_tree_contraction(graph, root);
    const auto second = rake_compress_tree_contraction(graph, root);
    REQUIRE_EQ(first, second);
    REQUIRE(valid_rake_compress_tree_contraction(graph, first));
    replay_independently(graph, first);

    std::size_t removed = 0;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if (vertex == root) {
        REQUIRE(!first.removal_round[vertex].has_value());
        REQUIRE(!first.removal_kind[vertex].has_value());
      } else {
        REQUIRE(first.removal_round[vertex].has_value());
        REQUIRE(first.removal_kind[vertex].has_value());
        ++removed;
      }
    }
    REQUIRE_EQ(removed + 1, n);
  }
}

TEST_CASE(rake_compress_ignores_weights_and_edge_insertion_order) {
  using namespace algorithms::graphs;
  std::vector<std::pair<Vertex, Vertex>> edges{{0,1},{1,2},{1,3},{3,4},{3,5},{5,6},{5,7}};
  Graph first(8, false), second(8, false);
  for (std::size_t index = 0; index < edges.size(); ++index) {
    first.add_edge(edges[index].first, edges[index].second,
                   static_cast<std::int64_t>(index + 1));
  }
  std::reverse(edges.begin(), edges.end());
  for (std::size_t index = 0; index < edges.size(); ++index) {
    second.add_edge(edges[index].first, edges[index].second,
                    -static_cast<std::int64_t>(index + 17));
  }
  REQUIRE_EQ(rake_compress_tree_contraction(first, 3),
             rake_compress_tree_contraction(second, 3));
}
