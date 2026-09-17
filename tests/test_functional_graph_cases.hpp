#pragma once

#include "algorithms/graphs/functional_graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::graphs::FunctionalGraphIndex;
using algorithms::graphs::Vertex;

struct OracleVertexInfo {
  Vertex entry{};
  std::size_t distance{};
  std::size_t cycle_length{};
  std::size_t entry_position{};
  std::vector<Vertex> cycle;
};

OracleVertexInfo oracle_info(const std::vector<Vertex>& successor,
                             const Vertex start) {
  const std::size_t n = successor.size();
  std::vector<std::size_t> first(n, n);
  std::vector<Vertex> path;
  Vertex current = start;
  while (first[current] == n) {
    first[current] = path.size();
    path.push_back(current);
    current = successor[current];
  }
  const std::size_t cycle_begin = first[current];
  std::vector<Vertex> cycle(path.begin() + static_cast<std::ptrdiff_t>(cycle_begin),
                                      path.end());
  const auto minimum_it = std::min_element(cycle.begin(), cycle.end());
  std::rotate(cycle.begin(), minimum_it, cycle.end());
  const Vertex entry = path[cycle_begin];
  const auto entry_it = std::find(cycle.begin(), cycle.end(), entry);
  return OracleVertexInfo{entry, cycle_begin, cycle.size(),
                          static_cast<std::size_t>(entry_it - cycle.begin()),
                          std::move(cycle)};
}

Vertex oracle_kth(const std::vector<Vertex>& successor,
                            Vertex vertex, std::uint64_t steps) {
  // Small-instance independent simulation with explicit cycle detection.
  const std::size_t n = successor.size();
  if (steps <= static_cast<std::uint64_t>(2U * n + 2U)) {
    for (std::uint64_t step = 0U; step < steps; ++step) {
      vertex = successor[vertex];
    }
    return vertex;
  }
  const OracleVertexInfo info = oracle_info(successor, vertex);
  if (steps < static_cast<std::uint64_t>(info.distance)) {
    for (std::uint64_t step = 0U; step < steps; ++step) {
      vertex = successor[vertex];
    }
    return vertex;
  }
  for (std::size_t step = 0U; step < info.distance; ++step) {
    vertex = successor[vertex];
  }
  steps -= static_cast<std::uint64_t>(info.distance);
  steps %= static_cast<std::uint64_t>(info.cycle_length);
  for (std::uint64_t step = 0U; step < steps; ++step) {
    vertex = successor[vertex];
  }
  return vertex;
}

std::optional<std::uint64_t> oracle_steps_to_reach(
    const std::vector<Vertex>& successor,
    const Vertex from, const Vertex to) {
  const std::size_t n = successor.size();
  Vertex current = from;
  for (std::size_t step = 0U; step <= 2U * n; ++step) {
    if (current == to) {
      return static_cast<std::uint64_t>(step);
    }
    current = successor[current];
  }
  return std::nullopt;
}

void verify_against_oracle(const std::vector<Vertex>& successor) {
  FunctionalGraphIndex index(successor);
  const std::size_t n = successor.size();
  REQUIRE_EQ(index.size(), n);
  if (n == 0U) {
    REQUIRE_EQ(index.component_count(), 0U);
    REQUIRE(index.cycles().empty());
    return;
  }

  std::vector<std::vector<Vertex>> expected_cycles;
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    const auto info = oracle_info(successor, vertex);
    if (std::find(expected_cycles.begin(), expected_cycles.end(), info.cycle) ==
        expected_cycles.end()) {
      expected_cycles.push_back(info.cycle);
    }
  }
  std::sort(expected_cycles.begin(), expected_cycles.end(),
            [](const auto& left, const auto& right) {
              return left.front() < right.front();
            });
  REQUIRE_EQ(index.cycles(), expected_cycles);
  REQUIRE_EQ(index.component_count(), expected_cycles.size());

  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    const auto info = oracle_info(successor, vertex);
    REQUIRE_EQ(index.successor(vertex), successor[vertex]);
    REQUIRE_EQ(index.cycle_entry(vertex), info.entry);
    REQUIRE_EQ(index.distance_to_cycle(vertex), info.distance);
    REQUIRE_EQ(index.cycle_length(vertex), info.cycle_length);
    REQUIRE_EQ(index.cycle_entry_position(vertex), info.entry_position);
    REQUIRE_EQ(index.cycles()[index.component(vertex)], info.cycle);

    for (std::uint64_t steps = 0U;
         steps <= static_cast<std::uint64_t>(2U * n + 5U); ++steps) {
      REQUIRE_EQ(index.kth_successor(vertex, steps),
                 oracle_kth(successor, vertex, steps));
    }
    REQUIRE_EQ(index.kth_successor(vertex,
                                   std::numeric_limits<std::uint64_t>::max()),
               oracle_kth(successor, vertex,
                          std::numeric_limits<std::uint64_t>::max()));

    for (Vertex target = 0U; target < n; ++target) {
      REQUIRE_EQ(index.steps_to_reach(vertex, target),
                 oracle_steps_to_reach(successor, vertex, target));
    }
  }
}

TEST_CASE(functional_graph_validates_and_handles_empty_singleton) {
  FunctionalGraphIndex empty({});
  REQUIRE_EQ(empty.size(), 0U);
  REQUIRE_EQ(empty.component_count(), 0U);
  REQUIRE_THROWS_AS(empty.kth_successor(0U, 0U), std::out_of_range);

  FunctionalGraphIndex single({0U});
  REQUIRE_EQ(single.size(), 1U);
  REQUIRE_EQ(single.component_count(), 1U);
  REQUIRE_EQ(single.cycles(), (std::vector<std::vector<Vertex>>{{0U}}));
  REQUIRE_EQ(single.kth_successor(0U, std::numeric_limits<std::uint64_t>::max()), 0U);
  REQUIRE_EQ(single.steps_to_reach(0U, 0U), std::optional<std::uint64_t>{0U});

  REQUIRE_THROWS_AS(FunctionalGraphIndex({1U}), std::out_of_range);
  REQUIRE_THROWS_AS(single.successor(1U), std::out_of_range);
  REQUIRE_THROWS_AS(single.steps_to_reach(0U, 1U), std::out_of_range);
}

TEST_CASE(functional_graph_decomposes_cycles_and_reverse_trees_deterministically) {
  const std::vector<Vertex> successor{
      1U, 2U, 0U, 4U, 4U, 4U, 7U, 6U, 7U, 8U};
  FunctionalGraphIndex index(successor);
  REQUIRE_EQ(index.cycles(),
             (std::vector<std::vector<Vertex>>{{0U, 1U, 2U}, {4U},
                                                          {6U, 7U}}));
  REQUIRE_EQ(index.component_count(), 3U);

  REQUIRE_EQ(index.distance_to_cycle(3U), 1U);
  REQUIRE_EQ(index.cycle_entry(3U), 4U);
  REQUIRE_EQ(index.distance_to_cycle(5U), 1U);
  REQUIRE_EQ(index.cycle_entry(5U), 4U);
  REQUIRE_EQ(index.distance_to_cycle(9U), 2U);
  REQUIRE_EQ(index.cycle_entry(9U), 7U);
  REQUIRE_EQ(index.cycle_entry_position(9U), 1U);

  REQUIRE_EQ(index.steps_to_reach(9U, 8U), std::optional<std::uint64_t>{1U});
  REQUIRE_EQ(index.steps_to_reach(9U, 7U), std::optional<std::uint64_t>{2U});
  REQUIRE_EQ(index.steps_to_reach(9U, 6U), std::optional<std::uint64_t>{3U});
  REQUIRE(!index.steps_to_reach(8U, 9U).has_value());
  REQUIRE(!index.steps_to_reach(0U, 4U).has_value());
}

TEST_CASE(functional_graph_exhaustive_small_successor_functions_match_oracle) {
  for (std::size_t n = 1U; n <= 5U; ++n) {
    std::uint64_t count = 1U;
    for (std::size_t index = 0U; index < n; ++index) {
      count *= static_cast<std::uint64_t>(n);
    }
    for (std::uint64_t code = 0U; code < count; ++code) {
      std::uint64_t value = code;
      std::vector<Vertex> successor(n, 0U);
      for (Vertex vertex = 0U; vertex < n; ++vertex) {
        successor[vertex] = static_cast<Vertex>(value % n);
        value /= n;
      }
      verify_against_oracle(successor);
    }
  }
}

TEST_CASE(functional_graph_random_larger_instances_match_oracle) {
  std::mt19937_64 rng(0xF00C710AULL);
  for (std::size_t trial = 0U; trial < 800U; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 40U);
    std::vector<Vertex> successor(n, 0U);
    for (Vertex vertex = 0U; vertex < n; ++vertex) {
      successor[vertex] = static_cast<Vertex>(rng() % n);
    }
    FunctionalGraphIndex index(successor);
    for (Vertex vertex = 0U; vertex < n; ++vertex) {
      const auto info = oracle_info(successor, vertex);
      REQUIRE_EQ(index.cycle_entry(vertex), info.entry);
      REQUIRE_EQ(index.distance_to_cycle(vertex), info.distance);
      REQUIRE_EQ(index.cycle_length(vertex), info.cycle_length);
      const std::uint64_t steps = rng();
      REQUIRE_EQ(index.kth_successor(vertex, steps),
                 oracle_kth(successor, vertex, steps));
      const Vertex target = static_cast<Vertex>(rng() % n);
      REQUIRE_EQ(index.steps_to_reach(vertex, target),
                 oracle_steps_to_reach(successor, vertex, target));
    }
  }
}

}  // namespace
