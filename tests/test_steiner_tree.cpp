#include "algorithms/graphs/steiner_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::SteinerTreeResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

struct OracleEdge {
  Vertex from;
  Vertex to;
  Weight weight;
};

class OracleDsu {
 public:
  explicit OracleDsu(std::size_t n) : parent_(n), size_(n, 1) {
    for (std::size_t i = 0; i < n; ++i) parent_[i] = i;
  }
  std::size_t find(std::size_t x) {
    while (parent_[x] != x) x = parent_[x];
    return x;
  }
  void unite(std::size_t a, std::size_t b) {
    a = find(a);
    b = find(b);
    if (a == b) return;
    if (size_[a] < size_[b]) std::swap(a, b);
    parent_[b] = a;
    size_[a] += size_[b];
  }
 private:
  std::vector<std::size_t> parent_;
  std::vector<std::size_t> size_;
};

std::vector<Vertex> canonical_terminals(std::vector<Vertex> terminals) {
  std::sort(terminals.begin(), terminals.end());
  terminals.erase(std::unique(terminals.begin(), terminals.end()), terminals.end());
  return terminals;
}

std::optional<Weight> exhaustive_optimum(std::size_t vertex_count,
                                         const std::vector<OracleEdge>& edges,
                                         std::vector<Vertex> terminals) {
  terminals = canonical_terminals(std::move(terminals));
  if (terminals.size() <= 1) return Weight{0};
  if (edges.size() >= std::numeric_limits<std::size_t>::digits) {
    throw std::logic_error("oracle edge mask too large");
  }
  const std::size_t subset_count = std::size_t{1} << edges.size();
  std::optional<Weight> best;
  for (std::size_t mask = 0; mask < subset_count; ++mask) {
    OracleDsu dsu(vertex_count);
    Weight cost = 0;
    bool overflow = false;
    for (std::size_t i = 0; i < edges.size(); ++i) {
      if ((mask & (std::size_t{1} << i)) == 0) continue;
      const auto& edge = edges[i];
      if (edge.weight > 0 && cost > std::numeric_limits<Weight>::max() - edge.weight) {
        overflow = true;
        break;
      }
      cost += edge.weight;
      dsu.unite(edge.from, edge.to);
    }
    if (overflow) continue;
    const std::size_t root = dsu.find(terminals.front());
    bool connected = true;
    for (Vertex terminal : terminals) {
      if (dsu.find(terminal) != root) {
        connected = false;
        break;
      }
    }
    if (connected && (!best.has_value() || cost < *best)) best = cost;
  }
  return best;
}

void replay_witness(const Graph& graph, const SteinerTreeResult& result) {
  REQUIRE_EQ(result.terminals, canonical_terminals(result.terminals));
  OracleDsu dsu(graph.vertex_count());
  Weight sum = 0;
  std::vector<std::pair<Vertex, std::size_t>> seen;
  for (const auto& witness : result.edges) {
    REQUIRE(witness.from < graph.vertex_count());
    const auto& adj = graph.neighbors(witness.from);
    REQUIRE(witness.adjacency_index < adj.size());
    const auto& original = adj[witness.adjacency_index];
    REQUIRE_EQ(original.to, witness.to);
    REQUIRE_EQ(original.weight, witness.weight);
    REQUIRE(std::find(seen.begin(), seen.end(),
                      std::pair<Vertex, std::size_t>{witness.from,
                                                     witness.adjacency_index}) ==
            seen.end());
    seen.push_back({witness.from, witness.adjacency_index});
    REQUIRE(witness.from != witness.to);
    REQUIRE(dsu.find(witness.from) != dsu.find(witness.to));
    dsu.unite(witness.from, witness.to);
    REQUIRE(witness.weight >= 0);
    REQUIRE(sum <= std::numeric_limits<Weight>::max() - witness.weight);
    sum += witness.weight;
  }
  REQUIRE_EQ(sum, result.total_weight);
  if (result.terminals.size() > 1) {
    const std::size_t root = dsu.find(result.terminals.front());
    for (Vertex terminal : result.terminals) REQUIRE_EQ(dsu.find(terminal), root);
  }
}

TEST_CASE(steiner_validation_and_trivial_semantics) {
  Graph directed(2, true);
  directed.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::minimum_steiner_tree(directed, {0, 1}),
                    std::invalid_argument);

  Graph negative(2, false);
  negative.add_edge(0, 1, -1);
  REQUIRE_THROWS_AS(algorithms::graphs::minimum_steiner_tree(negative, {0}),
                    std::invalid_argument);

  Graph graph(3, false);
  graph.add_edge(0, 1, 5);
  REQUIRE_THROWS_AS(algorithms::graphs::minimum_steiner_tree(graph, {3}),
                    std::out_of_range);

  const auto empty = algorithms::graphs::minimum_steiner_tree(graph, {});
  REQUIRE(empty.has_value());
  REQUIRE_EQ(empty->total_weight, Weight{0});
  REQUIRE(empty->edges.empty());
  REQUIRE(empty->terminals.empty());

  const auto singleton = algorithms::graphs::minimum_steiner_tree(graph, {2, 2, 2});
  REQUIRE(singleton.has_value());
  REQUIRE_EQ(singleton->terminals, std::vector<Vertex>{2});
  REQUIRE_EQ(singleton->total_weight, Weight{0});
  REQUIRE(singleton->edges.empty());
}

TEST_CASE(steiner_shared_center_parallel_and_determinism) {
  Graph graph(5, false);
  std::vector<OracleEdge> edges;
  for (Vertex leaf = 0; leaf < 4; ++leaf) {
    graph.add_edge(4, leaf, 7);
    edges.push_back({4, leaf, 7});
    graph.add_edge(4, leaf, 1);
    edges.push_back({4, leaf, 1});
  }
  for (Vertex leaf = 0; leaf < 4; ++leaf) {
    const Vertex next = (leaf + 1) % 4;
    graph.add_edge(leaf, next, 3);
    edges.push_back({leaf, next, 3});
  }
  const std::vector<Vertex> terminals{0, 1, 2, 3};
  const auto expected = exhaustive_optimum(5, edges, terminals);
  REQUIRE(expected.has_value());
  REQUIRE_EQ(*expected, Weight{4});
  const auto first = algorithms::graphs::minimum_steiner_tree(graph, terminals);
  const auto second = algorithms::graphs::minimum_steiner_tree(graph, terminals);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  REQUIRE_EQ(*first, *second);
  REQUIRE_EQ(first->total_weight, Weight{4});
  REQUIRE_EQ(first->edges.size(), std::size_t{4});
  replay_witness(graph, *first);
}

TEST_CASE(steiner_zero_cycle_disconnect_and_overflow_boundary) {
  Graph zero(3, false);
  zero.add_edge(0, 1, 0);
  zero.add_edge(1, 2, 0);
  zero.add_edge(2, 0, 0);
  const auto z = algorithms::graphs::minimum_steiner_tree(zero, {0, 1, 2});
  REQUIRE(z.has_value());
  REQUIRE_EQ(z->total_weight, Weight{0});
  REQUIRE_EQ(z->edges.size(), std::size_t{2});
  replay_witness(zero, *z);

  Graph disconnected(4, false);
  disconnected.add_edge(0, 1, 1);
  disconnected.add_edge(2, 3, 1);
  REQUIRE(!algorithms::graphs::minimum_steiner_tree(disconnected, {0, 3}).has_value());

  Graph overflow_only(3, false);
  overflow_only.add_edge(0, 1, std::numeric_limits<Weight>::max());
  overflow_only.add_edge(1, 2, 1);
  REQUIRE_THROWS_AS(
      algorithms::graphs::minimum_steiner_tree(overflow_only, {0, 2}),
      std::overflow_error);

  Graph fallback(3, false);
  fallback.add_edge(0, 1, std::numeric_limits<Weight>::max());
  fallback.add_edge(1, 2, 1);
  fallback.add_edge(0, 2, 7);
  const auto finite = algorithms::graphs::minimum_steiner_tree(fallback, {0, 2});
  REQUIRE(finite.has_value());
  REQUIRE_EQ(finite->total_weight, Weight{7});
  replay_witness(fallback, *finite);
}

TEST_CASE(steiner_randomized_differential_against_edge_subset_oracle) {
  std::mt19937_64 rng(0x57E1A3ULL);
  for (std::size_t trial = 0; trial < 450; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 8U);
    Graph graph(n, false);
    std::vector<OracleEdge> edges;
    const std::size_t edge_count = n == 0 ? 0 : static_cast<std::size_t>(rng() % 11U);
    for (std::size_t e = 0; e < edge_count; ++e) {
      const Vertex from = static_cast<Vertex>(rng() % n);
      const Vertex to = static_cast<Vertex>(rng() % n);
      const Weight weight = static_cast<Weight>(rng() % 10U);
      graph.add_edge(from, to, weight);
      edges.push_back({from, to, weight});
    }

    std::vector<Vertex> terminals;
    if (n != 0) {
      const std::size_t requests = static_cast<std::size_t>(rng() % 6U);
      for (std::size_t i = 0; i < requests; ++i) {
        terminals.push_back(static_cast<Vertex>(rng() % n));
      }
    }
    const std::vector<Vertex> canonical = canonical_terminals(terminals);
    const auto expected = exhaustive_optimum(n, edges, canonical);
    const auto actual = algorithms::graphs::minimum_steiner_tree(graph, terminals);
    REQUIRE_EQ(actual.has_value(), expected.has_value());
    if (actual.has_value()) {
      REQUIRE_EQ(actual->total_weight, *expected);
      REQUIRE_EQ(actual->terminals, canonical);
      replay_witness(graph, *actual);
    }
  }
}

}  // namespace
