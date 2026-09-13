#pragma once

#include "algorithms/graphs/tutte_berge.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace tutte_berge_test_detail {
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using Matching = std::vector<std::pair<Vertex, Vertex>>;

[[nodiscard]] std::vector<std::vector<bool>> simple_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex u = 0; u < n; ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      if (edge.to != u) {
        adjacent[u][edge.to] = true;
        adjacent[edge.to][u] = true;
      }
    }
  }
  return adjacent;
}

void enumerate_matchings(const std::vector<std::vector<bool>>& adjacent,
                         std::vector<bool>& used, Matching& current,
                         std::size_t& optimum) {
  Vertex first = used.size();
  for (Vertex v = 0; v < used.size(); ++v) {
    if (!used[v]) { first = v; break; }
  }
  if (first == used.size()) {
    optimum = std::max(optimum, current.size());
    return;
  }
  used[first] = true;
  enumerate_matchings(adjacent, used, current, optimum);
  used[first] = false;
  for (Vertex mate = first + 1U; mate < used.size(); ++mate) {
    if (used[mate] || !adjacent[first][mate]) continue;
    used[first] = true;
    used[mate] = true;
    current.emplace_back(first, mate);
    enumerate_matchings(adjacent, used, current, optimum);
    current.pop_back();
    used[first] = false;
    used[mate] = false;
  }
}

[[nodiscard]] std::size_t maximum_matching_cardinality(
    const std::vector<std::vector<bool>>& adjacent) {
  std::vector<bool> used(adjacent.size(), false);
  Matching current;
  std::size_t optimum = 0;
  enumerate_matchings(adjacent, used, current, optimum);
  return optimum;
}

[[nodiscard]] std::vector<std::vector<Vertex>> components_after_mask(
    const std::vector<std::vector<bool>>& adjacent, std::uint64_t removed_mask) {
  const std::size_t n = adjacent.size();
  std::vector<bool> seen(n, false);
  std::vector<std::vector<Vertex>> components;
  for (Vertex start = 0; start < n; ++start) {
    if (((removed_mask >> start) & 1ULL) != 0ULL || seen[start]) continue;
    std::queue<Vertex> q;
    std::vector<Vertex> component;
    seen[start] = true;
    q.push(start);
    while (!q.empty()) {
      const Vertex u = q.front(); q.pop(); component.push_back(u);
      for (Vertex v = 0; v < n; ++v) {
        if (((removed_mask >> v) & 1ULL) == 0ULL && adjacent[u][v] && !seen[v]) {
          seen[v] = true; q.push(v);
        }
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  }
  return components;
}

struct Oracle {
  std::size_t matching_cardinality{};
  std::size_t deficiency{};
};

[[nodiscard]] Oracle oracle(const Graph& graph) {
  const auto adjacent = simple_adjacency(graph);
  const std::size_t n = adjacent.size();
  REQUIRE(n <= 20U);
  const std::size_t matching = maximum_matching_cardinality(adjacent);
  std::int64_t best = std::numeric_limits<std::int64_t>::min();
  const std::uint64_t limit = 1ULL << n;
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    std::size_t odd = 0;
    for (const auto& component : components_after_mask(adjacent, mask)) {
      if (component.size() % 2U == 1U) ++odd;
    }
    const auto barrier_size = static_cast<std::int64_t>(std::popcount(mask));
    const auto value = static_cast<std::int64_t>(odd) - barrier_size;
    best = std::max(best, value);
  }
  REQUIRE(best >= 0);
  return Oracle{matching, static_cast<std::size_t>(best)};
}

[[nodiscard]] std::uint64_t mask_from_vertices(
    const std::vector<Vertex>& vertices) {
  std::uint64_t mask = 0;
  for (const Vertex v : vertices) mask |= (1ULL << v);
  return mask;
}

void check_certificate(const Graph& graph) {
  const auto cert = algorithms::graphs::tutte_berge_certificate(graph);
  const auto expected = oracle(graph);
  REQUIRE_EQ(cert.maximum_matching.cardinality, expected.matching_cardinality);
  REQUIRE_EQ(cert.deficiency, expected.deficiency);
  REQUIRE_EQ(cert.unmatched_vertex_count, expected.deficiency);
  REQUIRE_EQ(cert.gallai_blossom_calls, graph.vertex_count() + 1U);
  REQUIRE_EQ(2U * cert.maximum_matching.cardinality + cert.deficiency,
             graph.vertex_count());

  const auto adjacent = simple_adjacency(graph);
  const std::uint64_t barrier_mask = mask_from_vertices(cert.barrier_vertices);
  const auto components = components_after_mask(adjacent, barrier_mask);
  std::vector<std::vector<Vertex>> odd;
  std::vector<std::vector<Vertex>> even;
  for (const auto& component : components) {
    (component.size() % 2U == 1U ? odd : even).push_back(component);
  }
  REQUIRE_EQ(cert.odd_components, odd);
  REQUIRE_EQ(cert.even_components, even);
  REQUIRE_EQ(cert.odd_components.size() - cert.barrier_vertices.size(),
             cert.deficiency);

  std::size_t unmatched = 0;
  for (const auto& mate : cert.maximum_matching.mate) {
    if (!mate.has_value()) ++unmatched;
  }
  REQUIRE_EQ(unmatched, cert.unmatched_vertex_count);
}
}  // namespace tutte_berge_test_detail

TEST_CASE(tutte_berge_deterministic_certificates) {
  using algorithms::graphs::Graph;
  using tutte_berge_test_detail::check_certificate;
  Graph empty(0, false); check_certificate(empty);
  Graph singleton(1, false); check_certificate(singleton);
  Graph edge(2, false); edge.add_edge(0,1); check_certificate(edge);
  Graph odd_cycle(5, false); for(std::size_t v=0;v<5;++v) odd_cycle.add_edge(v,(v+1U)%5U); check_certificate(odd_cycle);
  Graph star(5, false); for(std::size_t v=1;v<5;++v) star.add_edge(0,v); check_certificate(star);
  Graph mixed(8, false); mixed.add_edge(0,1); mixed.add_edge(1,2); mixed.add_edge(2,0); mixed.add_edge(3,4); mixed.add_edge(5,6); mixed.add_edge(6,7); mixed.add_edge(7,5); check_certificate(mixed);
}

TEST_CASE(tutte_berge_multigraph_semantics_and_validation) {
  using algorithms::graphs::Graph;
  Graph graph(5, false);
  graph.add_edge(0,0,77); graph.add_edge(0,1,-9); graph.add_edge(0,1,13); graph.add_edge(1,2,5); graph.add_edge(2,3,-11); graph.add_edge(3,4,17);
  tutte_berge_test_detail::check_certificate(graph);
  Graph directed(2,true); directed.add_edge(0,1);
  REQUIRE_THROWS_AS(algorithms::graphs::tutte_berge_certificate(directed), std::invalid_argument);
}

TEST_CASE(tutte_berge_randomized_exhaustive_minmax_differential) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::Vertex;
  std::mt19937_64 rng(0x5455545445424552ULL);
  for (std::size_t trial=0; trial<450; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng()%9U);
    Graph graph(n,false);
    const std::size_t copies = static_cast<std::size_t>(rng()%20U);
    for(std::size_t e=0;e<copies && n!=0;++e){
      const Vertex u=static_cast<Vertex>(rng()%n); const Vertex v=static_cast<Vertex>(rng()%n);
      graph.add_edge(u,v,static_cast<std::int64_t>(rng()));
    }
    tutte_berge_test_detail::check_certificate(graph);
  }
}

TEST_CASE(tutte_berge_repeat_determinism) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::Vertex;
  Graph graph(9,false);
  const std::vector<std::pair<Vertex,Vertex>> edges{{0,1},{1,2},{2,0},{2,3},{3,4},{4,5},{5,3},{5,6},{6,7},{7,8}};
  for(const auto& [u,v]:edges) graph.add_edge(u,v);
  const auto a=algorithms::graphs::tutte_berge_certificate(graph);
  const auto b=algorithms::graphs::tutte_berge_certificate(graph);
  REQUIRE_EQ(a.maximum_matching.mate,b.maximum_matching.mate);
  REQUIRE_EQ(a.barrier_vertices,b.barrier_vertices);
  REQUIRE_EQ(a.odd_components,b.odd_components);
  REQUIRE_EQ(a.even_components,b.even_components);
  REQUIRE_EQ(a.deficiency,b.deficiency);
}
