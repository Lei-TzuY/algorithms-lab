#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/data_structures/disjoint_set_union.hpp"
#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/minimum_spanning_tree.hpp"
#include "algorithms/graphs/strongly_connected_components.hpp"
#include "algorithms/graphs/traversal.hpp"
#include "algorithms/greedy/interval_scheduling.hpp"

namespace {

using algorithms::data_structures::DisjointSetUnion;
using algorithms::graphs::Graph;
using algorithms::graphs::MinimumSpanningForest;
using algorithms::graphs::StronglyConnectedComponents;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;
using algorithms::greedy::Interval;

std::size_t brute_force_interval_optimum(const std::vector<Interval>& intervals) {
  const std::size_t n = intervals.size();
  REQUIRE(n <= 20);
  const std::uint64_t limit = std::uint64_t{1} << n;
  std::size_t best = 0;

  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    std::vector<Interval> chosen;
    std::size_t count = 0;
    for (std::size_t index = 0; index < n; ++index) {
      if ((mask & (std::uint64_t{1} << index)) != 0U) {
        chosen.push_back(intervals[index]);
        ++count;
      }
    }
    if (count <= best) {
      continue;
    }

    std::sort(chosen.begin(), chosen.end(),
              [](const Interval& left, const Interval& right) {
                if (left.start != right.start) {
                  return left.start < right.start;
                }
                return left.finish < right.finish;
              });

    bool compatible = true;
    for (std::size_t index = 1; index < chosen.size(); ++index) {
      if (chosen[index].start < chosen[index - 1].finish) {
        compatible = false;
        break;
      }
    }
    if (compatible) {
      best = count;
    }
  }
  return best;
}

void require_forest_shape(const MinimumSpanningForest& forest,
                          std::size_t vertex_count) {
  REQUIRE(forest.component_count <= vertex_count);
  REQUIRE_EQ(forest.edges.size(), vertex_count - forest.component_count);

  DisjointSetUnion dsu(vertex_count);
  for (const auto& edge : forest.edges) {
    REQUIRE(edge.from < vertex_count);
    REQUIRE(edge.to < vertex_count);
    REQUIRE(dsu.unite(edge.from, edge.to));
  }
  REQUIRE_EQ(dsu.components(), forest.component_count);
}

bool same_partition(const StronglyConnectedComponents& left,
                    const StronglyConnectedComponents& right) {
  if (left.component_of.size() != right.component_of.size()) {
    return false;
  }
  for (Vertex a = 0; a < left.component_of.size(); ++a) {
    for (Vertex b = 0; b < left.component_of.size(); ++b) {
      const bool left_same = left.component_of[a] == left.component_of[b];
      const bool right_same = right.component_of[a] == right.component_of[b];
      if (left_same != right_same) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

TEST_CASE(interval_scheduling_handles_boundaries_and_rejects_invalid_input) {
  const std::vector<Interval> intervals{
      {1, 4}, {3, 5}, {0, 6}, {5, 7}, {5, 9}, {8, 9}, {9, 9}, {9, 11}};
  const auto selected =
      algorithms::greedy::select_maximum_compatible_intervals(intervals);

  REQUIRE_EQ(selected.size(), std::size_t{5});
  for (std::size_t index = 1; index < selected.size(); ++index) {
    REQUIRE(intervals[selected[index]].start >=
            intervals[selected[index - 1]].finish);
  }

  const std::vector<Interval> empty;
  REQUIRE(algorithms::greedy::select_maximum_compatible_intervals(empty).empty());

  const std::vector<Interval> invalid{{5, 4}};
  REQUIRE_THROWS_AS(
      algorithms::greedy::select_maximum_compatible_intervals(invalid),
      std::invalid_argument);
}

TEST_CASE(interval_scheduling_matches_bruteforce_randomized_oracle) {
  std::mt19937_64 rng(0x1A2B3C4DULL);
  std::uniform_int_distribution<int> count_distribution(0, 11);
  std::uniform_int_distribution<int> start_distribution(-8, 12);
  std::uniform_int_distribution<int> length_distribution(1, 7);

  for (std::size_t trial = 0; trial < 350; ++trial) {
    const std::size_t count =
        static_cast<std::size_t>(count_distribution(rng));
    std::vector<Interval> intervals;
    intervals.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      const std::int64_t start = start_distribution(rng);
      const std::int64_t finish = start + length_distribution(rng);
      intervals.push_back(Interval{start, finish});
    }

    const auto selected =
        algorithms::greedy::select_maximum_compatible_intervals(intervals);
    REQUIRE_EQ(selected.size(), brute_force_interval_optimum(intervals));
  }
}

TEST_CASE(mst_known_graph_parallel_edges_self_loops_and_disconnected_components) {
  Graph connected(4, false);
  connected.add_edge(0, 1, 10);
  connected.add_edge(0, 2, 6);
  connected.add_edge(0, 3, 5);
  connected.add_edge(1, 3, 15);
  connected.add_edge(2, 3, 4);

  const auto kruskal =
      algorithms::graphs::kruskal_minimum_spanning_forest(connected);
  const auto prim =
      algorithms::graphs::prim_minimum_spanning_forest(connected);
  REQUIRE_EQ(kruskal.total_weight, Weight{19});
  REQUIRE_EQ(prim.total_weight, Weight{19});
  REQUIRE_EQ(kruskal.component_count, std::size_t{1});
  REQUIRE_EQ(prim.component_count, std::size_t{1});
  require_forest_shape(kruskal, connected.vertex_count());
  require_forest_shape(prim, connected.vertex_count());

  Graph forest_graph(5, false);
  forest_graph.add_edge(0, 1, 5);
  forest_graph.add_edge(0, 1, -2);
  forest_graph.add_edge(1, 1, -100);
  forest_graph.add_edge(2, 3, 0);
  forest_graph.add_edge(3, 4, 1);
  forest_graph.add_edge(2, 4, 9);

  const auto disconnected_kruskal =
      algorithms::graphs::kruskal_minimum_spanning_forest(forest_graph);
  const auto disconnected_prim =
      algorithms::graphs::prim_minimum_spanning_forest(forest_graph);
  REQUIRE_EQ(disconnected_kruskal.total_weight, Weight{-1});
  REQUIRE_EQ(disconnected_prim.total_weight, Weight{-1});
  REQUIRE_EQ(disconnected_kruskal.component_count, std::size_t{2});
  REQUIRE_EQ(disconnected_prim.component_count, std::size_t{2});
  require_forest_shape(disconnected_kruskal, forest_graph.vertex_count());
  require_forest_shape(disconnected_prim, forest_graph.vertex_count());
}

TEST_CASE(mst_rejects_directed_graphs_handles_empty_and_reports_overflow) {
  Graph directed(2, true);
  directed.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(
      algorithms::graphs::kruskal_minimum_spanning_forest(directed),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::graphs::prim_minimum_spanning_forest(directed),
      std::invalid_argument);

  Graph empty(0, false);
  const auto empty_kruskal =
      algorithms::graphs::kruskal_minimum_spanning_forest(empty);
  const auto empty_prim =
      algorithms::graphs::prim_minimum_spanning_forest(empty);
  REQUIRE_EQ(empty_kruskal.component_count, std::size_t{0});
  REQUIRE_EQ(empty_prim.component_count, std::size_t{0});
  REQUIRE(empty_kruskal.edges.empty());
  REQUIRE(empty_prim.edges.empty());

  Graph overflow(3, false);
  overflow.add_edge(0, 1, std::numeric_limits<Weight>::max());
  overflow.add_edge(1, 2, std::numeric_limits<Weight>::max());
  REQUIRE_THROWS_AS(
      algorithms::graphs::kruskal_minimum_spanning_forest(overflow),
      std::overflow_error);
  REQUIRE_THROWS_AS(
      algorithms::graphs::prim_minimum_spanning_forest(overflow),
      std::overflow_error);
}

TEST_CASE(kruskal_and_prim_match_on_randomized_multigraphs) {
  std::mt19937_64 rng(0xC0FFEE42ULL);
  std::uniform_int_distribution<int> vertex_distribution(0, 9);
  std::uniform_int_distribution<int> chance(0, 99);
  std::uniform_int_distribution<int> weight_distribution(-20, 20);

  for (std::size_t trial = 0; trial < 450; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(vertex_distribution(rng));
    Graph graph(vertex_count, false);

    for (Vertex from = 0; from < vertex_count; ++from) {
      if (chance(rng) < 15) {
        graph.add_edge(from, from, weight_distribution(rng));
      }
      for (Vertex to = from + 1; to < vertex_count; ++to) {
        if (chance(rng) < 38) {
          graph.add_edge(from, to, weight_distribution(rng));
          if (chance(rng) < 20) {
            graph.add_edge(from, to, weight_distribution(rng));
          }
        }
      }
    }

    const auto kruskal =
        algorithms::graphs::kruskal_minimum_spanning_forest(graph);
    const auto prim = algorithms::graphs::prim_minimum_spanning_forest(graph);

    REQUIRE_EQ(kruskal.total_weight, prim.total_weight);
    REQUIRE_EQ(kruskal.component_count, prim.component_count);
    require_forest_shape(kruskal, vertex_count);
    require_forest_shape(prim, vertex_count);
  }
}

TEST_CASE(scc_algorithms_identify_known_partition_and_condensation_dag) {
  Graph graph(8, true);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  graph.add_edge(2, 0);
  graph.add_edge(2, 3);
  graph.add_edge(3, 4);
  graph.add_edge(4, 5);
  graph.add_edge(5, 3);
  graph.add_edge(5, 6);
  graph.add_edge(6, 7);
  graph.add_edge(7, 6);
  graph.add_edge(1, 1);
  graph.add_edge(2, 3);

  const auto tarjan =
      algorithms::graphs::tarjan_strongly_connected_components(graph);
  const auto kosaraju =
      algorithms::graphs::kosaraju_strongly_connected_components(graph);

  REQUIRE(same_partition(tarjan, kosaraju));
  REQUIRE_EQ(tarjan.components.size(), std::size_t{3});
  REQUIRE(tarjan.component_of[0] == tarjan.component_of[2]);
  REQUIRE(tarjan.component_of[3] == tarjan.component_of[5]);
  REQUIRE(tarjan.component_of[6] == tarjan.component_of[7]);
  REQUIRE(tarjan.component_of[0] != tarjan.component_of[3]);
  REQUIRE(tarjan.component_of[3] != tarjan.component_of[6]);

  const Graph condensation = algorithms::graphs::condensation_graph(graph, tarjan);
  REQUIRE_EQ(condensation.vertex_count(), std::size_t{3});
  const auto order = algorithms::graphs::topological_sort(condensation);
  REQUIRE(order.has_value());
  REQUIRE_EQ(order->size(), condensation.vertex_count());
}

TEST_CASE(scc_rejects_undirected_graph_and_validates_decomposition) {
  Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(
      algorithms::graphs::tarjan_strongly_connected_components(undirected),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::graphs::kosaraju_strongly_connected_components(undirected),
      std::invalid_argument);

  Graph directed(2, true);
  directed.add_edge(0, 1);
  StronglyConnectedComponents malformed;
  malformed.component_of = {0};
  malformed.components = {{0}};
  REQUIRE_THROWS_AS(
      algorithms::graphs::condensation_graph(directed, malformed),
      std::invalid_argument);

  Graph empty(0, true);
  const auto empty_scc =
      algorithms::graphs::tarjan_strongly_connected_components(empty);
  REQUIRE(empty_scc.components.empty());
  REQUIRE(empty_scc.component_of.empty());
  const Graph empty_condensation =
      algorithms::graphs::condensation_graph(empty, empty_scc);
  REQUIRE_EQ(empty_condensation.vertex_count(), std::size_t{0});
}

TEST_CASE(tarjan_kosaraju_and_mutual_reachability_agree_randomized) {
  std::mt19937_64 rng(0x5CC0A11ULL);
  std::uniform_int_distribution<int> vertex_distribution(0, 9);
  std::uniform_int_distribution<int> chance(0, 99);

  for (std::size_t trial = 0; trial < 260; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(vertex_distribution(rng));
    Graph graph(vertex_count, true);

    for (Vertex from = 0; from < vertex_count; ++from) {
      for (Vertex to = 0; to < vertex_count; ++to) {
        if (chance(rng) < 24) {
          graph.add_edge(from, to);
          if (chance(rng) < 8) {
            graph.add_edge(from, to);
          }
        }
      }
    }

    const auto tarjan =
        algorithms::graphs::tarjan_strongly_connected_components(graph);
    const auto kosaraju =
        algorithms::graphs::kosaraju_strongly_connected_components(graph);
    REQUIRE(same_partition(tarjan, kosaraju));

    for (Vertex a = 0; a < vertex_count; ++a) {
      for (Vertex b = 0; b < vertex_count; ++b) {
        const bool same_component =
            tarjan.component_of[a] == tarjan.component_of[b];
        const bool mutually_reachable =
            algorithms::graphs::is_reachable(graph, a, b) &&
            algorithms::graphs::is_reachable(graph, b, a);
        REQUIRE(same_component == mutually_reachable);
      }
    }

    const Graph condensation =
        algorithms::graphs::condensation_graph(graph, tarjan);
    const auto order = algorithms::graphs::topological_sort(condensation);
    REQUIRE(order.has_value());
    REQUIRE_EQ(order->size(), condensation.vertex_count());
  }
}
