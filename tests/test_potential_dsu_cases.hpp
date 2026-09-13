#pragma once

#include "algorithms/data_structures/potential_dsu.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace {
using DSU = algorithms::data_structures::PotentialDisjointSetUnion;
using CR = DSU::ConstraintResult;

struct Arc {
  std::size_t to;
  std::int64_t delta;
};

std::optional<std::int64_t> oracle_difference(
    const std::vector<std::vector<Arc>>& graph, std::size_t from,
    std::size_t to) {
  std::vector<bool> seen(graph.size(), false);
  std::vector<std::int64_t> potential(graph.size(), 0);
  std::queue<std::size_t> q;
  seen[from] = true;
  q.push(from);
  while (!q.empty()) {
    const std::size_t u = q.front();
    q.pop();
    if (u == to) return potential[u];
    for (const Arc& arc : graph[u]) {
      if (!seen[arc.to]) {
        seen[arc.to] = true;
        potential[arc.to] = static_cast<std::int64_t>(potential[u] + arc.delta);
        q.push(arc.to);
      }
    }
  }
  return std::nullopt;
}

std::vector<std::size_t> oracle_component_sizes(
    const std::vector<std::vector<Arc>>& graph) {
  std::vector<std::size_t> sizes(graph.size(), 0U);
  std::vector<bool> seen(graph.size(), false);
  for (std::size_t start = 0; start < graph.size(); ++start) {
    if (seen[start]) continue;
    std::vector<std::size_t> members;
    std::queue<std::size_t> q;
    seen[start] = true;
    q.push(start);
    while (!q.empty()) {
      const std::size_t u = q.front();
      q.pop();
      members.push_back(u);
      for (const Arc& arc : graph[u]) {
        if (!seen[arc.to]) {
          seen[arc.to] = true;
          q.push(arc.to);
        }
      }
    }
    for (const std::size_t member : members) sizes[member] = members.size();
  }
  return sizes;
}

TEST_CASE(potential_dsu_basic_constraints_and_contradictions) {
  DSU dsu(5);
  REQUIRE_EQ(dsu.components(), std::size_t{5});
  REQUIRE_EQ(dsu.constrain(0, 1, 5), CR::merged);
  REQUIRE_EQ(dsu.constrain(1, 2, -2), CR::merged);
  REQUIRE_EQ(dsu.difference(0, 2), std::optional<std::int64_t>{3});
  REQUIRE_EQ(dsu.difference(2, 0), std::optional<std::int64_t>{-3});
  REQUIRE_EQ(dsu.constrain(0, 2, 3), CR::already_satisfied);
  REQUIRE_EQ(dsu.constrain(0, 2, 4), CR::contradiction);
  REQUIRE(!dsu.difference(0, 4).has_value());
  REQUIRE_EQ(dsu.component_size(1), std::size_t{3});
  REQUIRE_EQ(dsu.components(), std::size_t{3});
  REQUIRE(dsu.valid_structure());
}

TEST_CASE(potential_dsu_signed_boundaries_and_fail_closed_overflow) {
  DSU dsu(4);
  const auto max = std::numeric_limits<std::int64_t>::max();
  const auto min = std::numeric_limits<std::int64_t>::min();
  REQUIRE_EQ(dsu.constrain(0, 1, max), CR::merged);
  REQUIRE_EQ(dsu.difference(0, 1), std::optional<std::int64_t>{max});
  REQUIRE_THROWS_AS(dsu.constrain(1, 2, 1), std::overflow_error);
  REQUIRE(!dsu.connected(0, 2));
  REQUIRE_EQ(dsu.components(), std::size_t{3});
  REQUIRE(dsu.valid_structure());

  DSU lower(2);
  REQUIRE_EQ(lower.constrain(0, 1, min), CR::merged);
  REQUIRE_EQ(lower.difference(0, 1), std::optional<std::int64_t>{min});
  REQUIRE_THROWS_AS(lower.difference(1, 0), std::overflow_error);
  REQUIRE_EQ(lower.constrain(1, 0, 0), CR::contradiction);
  REQUIRE(lower.valid_structure());

  // Force union-by-size to choose the orientation that requires negating the
  // requested root offset. INT64_MIN cannot be negated, so the operation must
  // fail before either component is linked.
  DSU transactional(4);
  REQUIRE_EQ(transactional.constrain(1, 2, 0), CR::merged);
  REQUIRE_EQ(transactional.constrain(2, 3, 0), CR::merged);
  REQUIRE_THROWS_AS(transactional.constrain(0, 1, min), std::overflow_error);
  REQUIRE(!transactional.connected(0, 1));
  REQUIRE_EQ(transactional.components(), std::size_t{2});
  REQUIRE_EQ(transactional.component_size(0), std::size_t{1});
  REQUIRE_EQ(transactional.component_size(1), std::size_t{3});
  REQUIRE(transactional.valid_structure());

  DSU extrema(5);
  REQUIRE_EQ(extrema.constrain(0, 1, max), CR::merged);
  REQUIRE_EQ(extrema.constrain(2, 3, 0), CR::merged);
  REQUIRE_EQ(extrema.constrain(3, 4, 0), CR::merged);
  REQUIRE_THROWS_AS(extrema.constrain(0, 2, -1), std::overflow_error);
  REQUIRE(!extrema.connected(0, 2));
  REQUIRE_EQ(extrema.components(), std::size_t{2});
  REQUIRE_EQ(extrema.difference(0, 1), std::optional<std::int64_t>{max});
  REQUIRE(extrema.valid_structure());
}

TEST_CASE(potential_dsu_validation_and_empty) {
  DSU empty(0);
  REQUIRE(empty.valid_structure());
  REQUIRE_EQ(empty.components(), std::size_t{0});
  REQUIRE_THROWS_AS(empty.connected(0, 0), std::out_of_range);
  DSU one(1);
  REQUIRE_EQ(one.constrain(0, 0, 0), CR::already_satisfied);
  REQUIRE_EQ(one.constrain(0, 0, 1), CR::contradiction);
  REQUIRE_EQ(one.difference(0, 0), std::optional<std::int64_t>{0});
  REQUIRE(one.valid_structure());
}

TEST_CASE(potential_dsu_randomized_graph_differential) {
  constexpr std::size_t n = 32;
  DSU dsu(n);
  std::vector<std::vector<Arc>> graph(n);
  std::mt19937_64 rng(0x504f54454e544941ULL);

  for (std::size_t step = 0; step < 30000U; ++step) {
    const std::size_t a = static_cast<std::size_t>(rng() % n);
    const std::size_t b = static_cast<std::size_t>(rng() % n);
    const std::int64_t delta = static_cast<std::int64_t>(rng() % 101U) - 50;
    const auto before = oracle_difference(graph, a, b);
    const CR result = dsu.constrain(a, b, delta);
    if (!before.has_value()) {
      REQUIRE_EQ(result, CR::merged);
      graph[a].push_back(Arc{b, delta});
      graph[b].push_back(Arc{a, static_cast<std::int64_t>(-delta)});
    } else if (*before == delta) {
      REQUIRE_EQ(result, CR::already_satisfied);
    } else {
      REQUIRE_EQ(result, CR::contradiction);
    }

    for (std::size_t probe = 0; probe < 4U; ++probe) {
      const std::size_t x = static_cast<std::size_t>(rng() % n);
      const std::size_t y = static_cast<std::size_t>(rng() % n);
      REQUIRE_EQ(dsu.difference(x, y), oracle_difference(graph, x, y));
    }
    if (step % 113U == 0U) {
      REQUIRE(dsu.valid_structure());
      const auto sizes = oracle_component_sizes(graph);
      std::size_t component_count = 0U;
      for (std::size_t v = 0; v < n; ++v) {
        REQUIRE_EQ(dsu.component_size(v), sizes[v]);
        bool smallest = true;
        for (std::size_t u = 0; u < v; ++u) {
          if (oracle_difference(graph, u, v).has_value()) {
            smallest = false;
            break;
          }
        }
        if (smallest) ++component_count;
      }
      REQUIRE_EQ(dsu.components(), component_count);
    }
  }
  REQUIRE(dsu.valid_structure());
}

}  // namespace
